#include "song_decoder_controller.h"

#define LOG_TAG "LiveSongDecoderController"
LiveSongDecoderController::LiveSongDecoderController() {
	accompanyDecoder = NULL;
	resampler = NULL;
	silentSamples = NULL;
	audioEffectProcessor = NULL;
}

LiveSongDecoderController::~LiveSongDecoderController() {
}

void LiveSongDecoderController::setAudioEffect(AudioEffect* audioEffectParam) {
	LOGI("enter LiveSongDecoderController::setAudioEffect()");
	if (audioEffectProcessor == NULL) {
		audioEffectProcessor = AudioEffectProcessorFactory::GetInstance()->buildAccompanyEffectProcessor();
		audioEffectProcessor->init(audioEffectParam);
	}
	audioEffectProcessor->setAudioEffect(audioEffectParam);
}

void* LiveSongDecoderController::startDecoderThread(void* ptr) {
	LOGI("enter LiveSongDecoderController::startDecoderThread");
	LiveSongDecoderController* decoderController = (LiveSongDecoderController *) ptr;
	int getLockCode = pthread_mutex_lock(&decoderController->mLock);
	while (decoderController->isRunning) {
		if (decoderController->isSuspendFlag) {
			pthread_mutex_lock(&decoderController->mSuspendLock);
			pthread_cond_signal(&decoderController->mSuspendCondition);
			pthread_mutex_unlock(&decoderController->mSuspendLock);

			pthread_cond_wait(&decoderController->mCondition, &decoderController->mLock);
		} else {
			decoderController->decodeSongPacket();
			if (decoderController->packetPool->geDecoderAccompanyPacketQueueSize() > QUEUE_SIZE_MAX_THRESHOLD) {
				decoderController->accompanyType = ACCOMPANY_TYPE_SONG_SAMPLE;
				pthread_cond_wait(&decoderController->mCondition, &decoderController->mLock);
			}
		}
	}
	pthread_mutex_unlock(&decoderController->mLock);

	return NULL;
}

void LiveSongDecoderController::init(float packetBufferTimePercent, int vocalSampleRate) {
	//初始化两个全局变量
	volume = 1.0f;
	accompanyMax = 1.0f;
	accompanyType = ACCOMPANY_TYPE_SILENT_SAMPLE;
	this->vocalSampleRate = vocalSampleRate;

	//计算计算出伴奏和原唱的bufferSize
	int accompanyByteCountPerSec = vocalSampleRate * CHANNEL_PER_FRAME * BITS_PER_CHANNEL / BITS_PER_BYTE;
	accompanyPacketBufferSize = (int) ((accompanyByteCountPerSec / 2) * packetBufferTimePercent);

	//1s的数据与4个buffer的大小哪一个, 选择大的去分配空间
	bufferQueueCursor = 0;
	bufferQueueSize = MAX(accompanyPacketBufferSize * 4, vocalSampleRate * CHANNEL_PER_FRAME * 1.0f);
	bufferQueue = new short[bufferQueueSize];

	silentSamples = new short[accompanyPacketBufferSize];
	memset(silentSamples, 0 , accompanyPacketBufferSize * 2);
	//初始化队列以及开启线程
	packetPool = LiveCommonPacketPool::GetInstance();
	packetPool->initDecoderAccompanyPacketQueue();
	packetPool->initAccompanyPacketQueue(vocalSampleRate, CHANNEL_PER_FRAME);
	initDecoderThread();
	LOGI("after init Decoder thread...");
}

void LiveSongDecoderController::startAccompany(const char* accompanyPath){
	this->suspendDecoderThread();
	usleep(200 * 1000);
	if(this->initAccompanyDecoder(accompanyPath) >= 0){
		this->resumeDecoderThread();
	}
}

void LiveSongDecoderController::suspendDecoderThread(){
	pthread_mutex_lock(&mSuspendLock);
	pthread_mutex_lock(&mLock);
	isSuspendFlag = true;
	pthread_cond_signal(&mCondition);
	pthread_mutex_unlock(&mLock);
	pthread_cond_wait(&mSuspendCondition, &mSuspendLock);
	pthread_mutex_unlock(&mSuspendLock);
	accompanyType = ACCOMPANY_TYPE_SILENT_SAMPLE;
}

int LiveSongDecoderController::initAccompanyDecoder(const char* accompanyPath) {
	packetPool->clearDecoderAccompanyPacketToQueue();
	this->destroyResampler();
	this->destroyAccompanyDecoder();
	accompanyDecoder = new AACAccompanyDecoder();
	int actualAccompanyPacketBufferSize = accompanyPacketBufferSize;
	int ret = accompanyDecoder->init(accompanyPath);
	if(ret > 0){
		//初始化伴奏的采样率
		accompanySampleRate = accompanyDecoder->getSampleRate();
		if (vocalSampleRate != accompanySampleRate) {
			isNeedResample = true;
			resampler = new Resampler();
			float ratio = (float)accompanySampleRate / (float)vocalSampleRate;
			actualAccompanyPacketBufferSize = ratio * accompanyPacketBufferSize;
			ret = resampler->init(accompanySampleRate, vocalSampleRate, actualAccompanyPacketBufferSize / 2, 2, 2);
			if (ret < 0) {
				LOGI("resampler init error\n");
			}
		} else {
			isNeedResample = false;
		}
		accompanyDecoder->setPacketBufferSize(actualAccompanyPacketBufferSize);
	}
	return ret;
}

void LiveSongDecoderController::resumeDecoderThread(){
	isSuspendFlag = false;
	int getLockCode = pthread_mutex_lock(&mLock);
	pthread_cond_signal (&mCondition);
	pthread_mutex_unlock (&mLock);
}

void LiveSongDecoderController::pauseAccompany(){
	this->suspendDecoderThread();
}

void LiveSongDecoderController::resumeAccompany(){
	this->resumeDecoderThread();
}

void LiveSongDecoderController::seek(float seconds) {
	if (accompanyDecoder) {
		accompanyDecoder->setPosition(seconds);
	}
}

void LiveSongDecoderController::stopAccompany(){
	this->suspendDecoderThread();
	packetPool->clearDecoderAccompanyPacketToQueue();
	this->destroyResampler();
	this->destroyAccompanyDecoder();
}

void LiveSongDecoderController::initDecoderThread() {
	isRunning = true;
	isSuspendFlag = true;
	pthread_mutex_init(&mLock, NULL);
	pthread_cond_init(&mCondition, NULL);
	pthread_mutex_init(&mSuspendLock, NULL);
	pthread_cond_init(&mSuspendCondition, NULL);
	pthread_create(&songDecoderThread, NULL, startDecoderThread, this);
}

void LiveSongDecoderController::decodeSongPacket() {
	if(NULL == accompanyDecoder){
		return;
	}
	LiveAudioPacket* accompanyPacket = accompanyDecoder->decodePacket();
	//是否需要重采样
	if (isNeedResample && NULL != resampler) {
		short* stereoSamples = accompanyPacket->buffer;
		int stereoSampleSize = accompanyPacket->size;
		if (stereoSampleSize > 0) {
			int monoSampleSize = stereoSampleSize / 2;
			short** samples = new short*[2];
			samples[0] = new short[monoSampleSize];
			samples[1] = new short[monoSampleSize];
			for (int i = 0; i < monoSampleSize; i++) {
				samples[0][i] = stereoSamples[2 * i];
				samples[1][i] = stereoSamples[2 * i + 1];
			}
			float transfer_ratio = (float) accompanySampleRate / (float) vocalSampleRate;
			int accompanySampleSize = (int) ((float) (monoSampleSize) / transfer_ratio);
//			LOGI("monoSampleSize is %d accompanySampleSize is %d", monoSampleSize, accompanySampleSize);
			uint8_t out_data[accompanySampleSize * 2 * 2];
			int out_nb_bytes = 0;
			resampler->process(samples, out_data, monoSampleSize, &out_nb_bytes);
			delete[] samples[0];
			delete[] samples[1];
			delete[] stereoSamples;
			if (out_nb_bytes > 0) {
				//			LOGI("out_nb_bytes is %d accompanySampleSize is %d", out_nb_bytes, accompanySampleSize);
				accompanySampleSize = out_nb_bytes / 2;
				short* accompanySamples = new short[accompanySampleSize];
				convertShortArrayFromByteArray(out_data, out_nb_bytes, accompanySamples, 1.0);
				accompanyPacket->buffer = accompanySamples;
				accompanyPacket->size = accompanySampleSize;
			}
		}
	}
	packetPool->pushDecoderAccompanyPacketToQueue(accompanyPacket);
}

void LiveSongDecoderController::destroyDecoderThread() {
//	LOGI("enter LiveSongDecoderController::destoryProduceThread ....");
	isRunning = false;
	isSuspendFlag = false;
	void* status;
	int getLockCode = pthread_mutex_lock(&mLock);
	pthread_cond_signal (&mCondition);
	pthread_mutex_unlock (&mLock);
	pthread_join(songDecoderThread, &status);
	pthread_mutex_destroy(&mLock);
	pthread_cond_destroy(&mCondition);

	pthread_mutex_lock(&mSuspendLock);
	pthread_cond_signal(&mSuspendCondition);
	pthread_mutex_unlock(&mSuspendLock);
	pthread_mutex_destroy(&mSuspendLock);
	pthread_cond_destroy(&mSuspendCondition);
}

int LiveSongDecoderController::buildSlientSamples(short* samples) {
	LiveAudioPacket* accompanyPacket = new LiveAudioPacket();
	int samplePacketSize = accompanyPacketBufferSize;
	accompanyPacket->size = samplePacketSize;
	accompanyPacket->buffer = new short[samplePacketSize];
	accompanyPacket->position = -1;
	memcpy(accompanyPacket->buffer, silentSamples, samplePacketSize * 2);
	memcpy(samples, accompanyPacket->buffer, samplePacketSize * 2);
	this->pushAccompanyPacketToQueue(accompanyPacket);
	return samplePacketSize;
}

int LiveSongDecoderController::readSamplesAndProducePacket(short* samples, int size, int* slientSizeArr, int * extraAccompanyTypeArr) {
	int result = -1;
	if(accompanyType == ACCOMPANY_TYPE_SILENT_SAMPLE){
		extraAccompanyTypeArr[0] = ACCOMPANY_TYPE_SILENT_SAMPLE;
		slientSizeArr[0] = 0;
		result = this->buildSlientSamples(samples);
	} else if (accompanyType == ACCOMPANY_TYPE_SONG_SAMPLE) {
		extraAccompanyTypeArr[0] = ACCOMPANY_TYPE_SONG_SAMPLE;
		LiveAudioPacket* accompanyPacket = NULL;
		packetPool->getDecoderAccompanyPacket(&accompanyPacket, true);
		if (NULL != accompanyPacket) {
			int samplePacketSize = accompanyPacket->size;
			if (samplePacketSize != -1 && samplePacketSize <= size) {
				//copy the raw data to samples
				adjustSamplesVolume(accompanyPacket->buffer, samplePacketSize, volume / accompanyMax);
				if (audioEffectProcessor != NULL) {
					AudioResponse* response = audioEffectProcessor->processAccompany(accompanyPacket->buffer, samplePacketSize, 0, 0);
					int *soundTouchReceiveSamples = (int *) response->get(ACCOMPANYRESPONSE_KEY_RECEIVESAMPLES_SIZE);
					if (soundTouchReceiveSamples != NULL) {
						if (*soundTouchReceiveSamples >= 0) {
							samplePacketSize = *soundTouchReceiveSamples;
						}
						delete soundTouchReceiveSamples;
					}
					//由于被PitchShift吞掉一些Sample所以这里更新AccompanyPacket的size
//					LOGI("PitchShift吞掉一些Sample {%d--->%d}", accompanyPacket->size, samplePacketSize);
					accompanyPacket->size = samplePacketSize;
					int accompanyPitchShiftUnProcessSample = 0;
					int *lastSoundTouchUnprocessSamples = (int *) response->get(ACCOMPANYRESPONSE_KEY_PITCHSHIFT_UNPROCESS_SIZE);
					if (lastSoundTouchUnprocessSamples != NULL) {
						response->deleteKey(ACCOMPANYRESPONSE_KEY_PITCHSHIFT_UNPROCESS_SIZE);
						accompanyPitchShiftUnProcessSample = *lastSoundTouchUnprocessSamples;
						delete lastSoundTouchUnprocessSamples;
					}
					slientSizeArr[0] = accompanyPitchShiftUnProcessSample;
					if(accompanyPitchShiftUnProcessSample > 0){
//						LOGI("由于重新变调再也找不回来的Sample个数 {%d}", accompanyPitchShiftUnProcessSample);
						//重新分配Accompany Packet
						int reAllocatedSize = accompanyPacket->size + accompanyPitchShiftUnProcessSample;
						short* buffer = new short[reAllocatedSize];
						memset(buffer, 0 , reAllocatedSize * sizeof(short));
						memcpy(buffer, accompanyPacket->buffer, accompanyPacket->size * sizeof(short));
						delete accompanyPacket;
						accompanyPacket = new LiveAudioPacket();
						accompanyPacket->size = reAllocatedSize;
						accompanyPacket->buffer = buffer;
					}
				}
				memcpy(samples, accompanyPacket->buffer, samplePacketSize * 2);
				//push accompany packet to accompany queue
				this->pushAccompanyPacketToQueue(accompanyPacket);
				result = samplePacketSize;
			} else if(-1 == samplePacketSize){
				//解码到最后了
				accompanyType = ACCOMPANY_TYPE_SILENT_SAMPLE;
				extraAccompanyTypeArr[0] = ACCOMPANY_TYPE_SILENT_SAMPLE;
				slientSizeArr[0] = 0;
				result = this->buildSlientSamples(samples);
			}
		} else {
			result = -2;
		}
		if (extraAccompanyTypeArr[0] != ACCOMPANY_TYPE_SILENT_SAMPLE && packetPool->geDecoderAccompanyPacketQueueSize() < QUEUE_SIZE_MIN_THRESHOLD) {
			int getLockCode = pthread_mutex_lock(&mLock);
			if (result != -1) {
				pthread_cond_signal(&mCondition);
			}
			pthread_mutex_unlock(&mLock);
		}
	}
	return result;
}

void LiveSongDecoderController::pushAccompanyPacketToQueue(LiveAudioPacket* tempPacket) {
	memcpy(bufferQueue + bufferQueueCursor, tempPacket->buffer, tempPacket->size * sizeof(short));
	bufferQueueCursor+=tempPacket->size;
	float position = tempPacket->position;
	delete tempPacket;
	while(bufferQueueCursor >= accompanyPacketBufferSize){
		short* buffer = new short[accompanyPacketBufferSize];
		memcpy(buffer, bufferQueue, accompanyPacketBufferSize * sizeof(short));
		int protectedSampleSize = bufferQueueCursor - accompanyPacketBufferSize;
		memmove(bufferQueue, bufferQueue + accompanyPacketBufferSize, protectedSampleSize * sizeof(short));
		bufferQueueCursor -= accompanyPacketBufferSize;
		LiveAudioPacket* actualPacket = new LiveAudioPacket();
		actualPacket->size = accompanyPacketBufferSize;
		actualPacket->buffer = buffer;
		actualPacket->position = position;
		if(position != -1){
			actualPacket->frameNum = position * this->vocalSampleRate;
		} else{
			actualPacket->frameNum = -1;
		}
		packetPool->pushAccompanyPacketToQueue(actualPacket);
	}
}

void LiveSongDecoderController::destroy() {
	LOGI("enter LiveSongDecoderController::destroy");
	this->destroyDecoderThread();
	LOGI("after destroyDecoderThread");
	packetPool->abortDecoderAccompanyPacketQueue();
	packetPool->destoryDecoderAccompanyPacketQueue();
	this->destroyResampler();
	this->destroyAccompanyDecoder();
	if (audioEffectProcessor != NULL) {
		audioEffectProcessor->destroy();
		delete audioEffectProcessor;
		audioEffectProcessor = NULL;
	}
	if (NULL != silentSamples) {
		delete[] silentSamples;
	}
	LOGI("leave LiveSongDecoderController::destroy");
}

void LiveSongDecoderController::destroyAccompanyDecoder() {
	if (NULL != accompanyDecoder) {
		accompanyDecoder->destroy();
		delete accompanyDecoder;
		accompanyDecoder = NULL;
	}
}

void LiveSongDecoderController::destroyResampler() {
	if (NULL != resampler) {
		resampler->destroy();
		delete resampler;
		resampler = NULL;
	}
}
