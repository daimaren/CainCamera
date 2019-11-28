#include "./audio_output.h"

#define LOG_TAG "AudioOutput"

AudioOutput::AudioOutput() {
}

AudioOutput::~AudioOutput() {
}

SLresult AudioOutput::registerPlayerCallback() {
	// Register the player callback
	//这里非常有意思的是，当 audioPlayerBufferQueue 可以接受音频数据的时候 ,就会收到 playCallback 的回调
	//所以音频播放器就可以不断通过该回调，客户端收到回调就 enqueue 数据到bufferQueue中，就可以实现不断播放了
	//如果是暂停的话，这里就停止回调，直到恢复，从而控制音频的播放，因为解码依赖于音频，所以视频画面也会被停止
	return (*audioPlayerBufferQueue)->RegisterCallback(audioPlayerBufferQueue, playerCallback, this); // player context
}

void AudioOutput::playerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
	AudioOutput* audioOutput = (AudioOutput*) context;
	audioOutput->producePacket();
}
void AudioOutput::producePacket() {
	//回调playerController中的方法来获得buffer
	if (playingState == PLAYING_STATE_PLAYING) {
		//去音频数据队列里面检查数据，数据不够就继续解码
		int actualSize = produceDataCallback(buffer, bufferSize, ctx);
		if (actualSize > 0 && playingState == PLAYING_STATE_PLAYING) {
			//将提供的数据加入到播放的buffer中去
			(*audioPlayerBufferQueue)->Enqueue(audioPlayerBufferQueue, buffer, actualSize);
		}
	}
}

SLresult AudioOutput::pause() {
	return setAudioPlayerStatePaused();
}

SLresult AudioOutput::stop() {
	ALOGI("Set the audio player state paused");
	// Set the audio player state playing
	SLresult result = setAudioPlayerStatePaused();
	if (SL_RESULT_SUCCESS != result) {
		return result;
	}
	playingState = PLAYING_STATE_STOPPED;
	usleep(0.05 * 1000000);
	ALOGI("destroyContext...");
	destroyContext();
	return SL_RESULT_SUCCESS;
}

SLresult AudioOutput::play() {
//	ALOGI("Set the audio player state playing");
	// Set the audio player state playing
	SLresult result = setAudioPlayerStatePlaying();
	if (SL_RESULT_SUCCESS != result) {
		return result;
	}
	playingState = PLAYING_STATE_PLAYING;
	return SL_RESULT_SUCCESS;
}

SLresult AudioOutput::start() {
//	ALOGI("Set the audio player state start");
	// Set the audio player state playing
	SLresult result = setAudioPlayerStatePlaying();
	if (SL_RESULT_SUCCESS != result) {
		return result;
	}
	playingState = PLAYING_STATE_PLAYING;
//	ALOGI(" Enqueue the first buffer to start");
	// Enqueue the first buffer to start
	producePacket();
	return SL_RESULT_SUCCESS;
}

SLresult AudioOutput::initSoundTrack(int channels, int accompanySampleRate, audioPlayerCallback produceDataCallback, void* ctx) {
	ALOGI("enter AudioOutput::initSoundTrack");

//	ALOGI("get open sl es Engine");
	this->ctx = ctx;
	this->produceDataCallback = produceDataCallback;
	SLresult result = SL_RESULT_UNKNOWN_ERROR;
	OpenSLESContext* openSLESContext = OpenSLESContext::GetInstance();
	engineEngine = openSLESContext->getEngine();

	if (engineEngine == NULL){
		return result;
	}

	ALOGI("Create output mix object");
	// Create output mix object
	result = createOutputMix();
	if (SL_RESULT_SUCCESS != result) {
		return result;
	}

	ALOGI("Realize output mix object");
	// Realize output mix object
	result = realizeObject(outputMixObject);
	if (SL_RESULT_SUCCESS != result) {
		return result;
	}

	ALOGI("Initialize buffer");
	//Calculate the buffer size default 50ms length
	bufferSize = channels * accompanySampleRate * 2 * DEFAULT_AUDIO_BUFFER_DURATION_IN_SECS;
	// Initialize buffer
	initPlayerBuffer();

	ALOGI("Create the buffer queue audio player object");
	// Create the buffer queue audio player object
	result = createAudioPlayer(channels, accompanySampleRate);
	if (SL_RESULT_SUCCESS != result) {
		freePlayerBuffer();
		destroyObject(outputMixObject);
		return result;
	}

	ALOGI("Realize audio player object");
	// Realize audio player object
	result = realizeObject(audioPlayerObject);
	if (SL_RESULT_SUCCESS != result) {
		freePlayerBuffer();
		destroyObject(outputMixObject);
		return result;
	}

	ALOGI("Get audio player buffer queue interface");
	// Get audio player buffer queue interface
	result = getAudioPlayerBufferQueueInterface();
	if (SL_RESULT_SUCCESS != result) {
		destroyObject(audioPlayerObject);
		freePlayerBuffer();
		destroyObject(outputMixObject);
		return result;
	}

	ALOGI("Registers the player callback");
	// Registers the player callback
	result = registerPlayerCallback();
	if (SL_RESULT_SUCCESS != result) {
		destroyObject(audioPlayerObject);
		freePlayerBuffer();
		destroyObject(outputMixObject);
		return result;
	}

	ALOGI("Get audio player play interface");
	// Get audio player play interface
	result = getAudioPlayerPlayInterface();
	if (SL_RESULT_SUCCESS != result) {
		destroyObject(audioPlayerObject);
		freePlayerBuffer();
		destroyObject(outputMixObject);
		return result;
	}	ALOGI("leave init");
	return SL_RESULT_SUCCESS;
}

long AudioOutput::getCurrentTimeMills() {
	SLmillisecond position = 0;
	if (0 != audioPlayerObject && NULL != (*audioPlayerPlay)) {
		SLresult result = (*audioPlayerPlay)->GetPosition(audioPlayerPlay, &position);
	}
	return position;
}

bool AudioOutput::isPlaying() {
	bool result = false;
	SLuint32 pState = SL_PLAYSTATE_PLAYING;
	if (0 != audioPlayerObject && NULL != (*audioPlayerPlay)) {
		SLresult result = (*audioPlayerPlay)->GetPlayState(audioPlayerPlay, &pState);
	} else {
		result = false;
	}
	if (pState == SL_PLAYSTATE_PLAYING) {
		result = true;
	}
	return result;
}

void AudioOutput::destroyContext() {
	ALOGI("enter AudioOutput::DestroyContext");
	// Destroy audio player object
	destroyObject(audioPlayerObject);
	ALOGI("after destroy audioPlayerObject");
	// Free the player buffer
	freePlayerBuffer();
	ALOGI("after FreePlayerBuffer");
	// Destroy output mix object
	destroyObject(outputMixObject);
	ALOGI("leave AudioOutput::DestroyContext");
}

/** 以下是私有方法的实现 **/
SLresult AudioOutput::realizeObject(SLObjectItf object) {
	return (*object)->Realize(object, SL_BOOLEAN_FALSE); // No async, blocking call
}

void AudioOutput::destroyObject(SLObjectItf& object) {
	if (0 != object)
		(*object)->Destroy(object);
	object = 0;
}

SLresult AudioOutput::createOutputMix() {
	// Create output mix object
	return (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, // no interfaces
			0, // no interfaces
			0); // no required
}

void AudioOutput::initPlayerBuffer() {
	//Initialize buffer
	buffer = new byte[bufferSize];
}

void AudioOutput::freePlayerBuffer() {
	if (NULL != buffer) {
		delete[] buffer;
		buffer = NULL;
	}
}

SLresult AudioOutput::createAudioPlayer(int channels, int accompanySampleRate) {
	SLDataLocator_AndroidSimpleBufferQueue dataSourceLocator = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, // locator type
			1 // buffer count
			};
	int samplesPerSec = opensl_get_sample_rate(accompanySampleRate);
	int channelMask = getChannelMask(channels);
	SLDataFormat_PCM dataSourceFormat = { SL_DATAFORMAT_PCM, // format type
            (SLuint32)channels, // channel count
            (SLuint32)samplesPerSec, // samples per second in millihertz
			SL_PCMSAMPLEFORMAT_FIXED_16, // bits per sample
			SL_PCMSAMPLEFORMAT_FIXED_16, // container size
            (SLuint32)channelMask, // channel mask
			SL_BYTEORDER_LITTLEENDIAN // endianness
			};

	// Data source is a simple buffer queue with PCM format
	SLDataSource dataSource = { &dataSourceLocator, // data locator
			&dataSourceFormat // data format
			};

	// Output mix locator for data sink
	SLDataLocator_OutputMix dataSinkLocator = { SL_DATALOCATOR_OUTPUTMIX, // locator type
			outputMixObject // output mix
			};

	// Data sink is an output mix
	SLDataSink dataSink = { &dataSinkLocator, // locator
			0 // format
			};

	// Interfaces that are requested
	SLInterfaceID interfaceIds[] = { SL_IID_BUFFERQUEUE };

	// Required interfaces. If the required interfaces
	// are not available the request will fail
	SLboolean requiredInterfaces[] = { SL_BOOLEAN_TRUE // for SL_IID_BUFFERQUEUE
			};

	// Create audio player object
	return (*engineEngine)->CreateAudioPlayer(engineEngine, &audioPlayerObject, &dataSource, &dataSink, ARRAY_LEN(interfaceIds), interfaceIds, requiredInterfaces);
}

SLresult AudioOutput::getAudioPlayerBufferQueueInterface() {
	return (*audioPlayerObject)->GetInterface(audioPlayerObject, SL_IID_BUFFERQUEUE, &audioPlayerBufferQueue);
}

SLresult AudioOutput::getAudioPlayerPlayInterface() {
	return (*audioPlayerObject)->GetInterface(audioPlayerObject, SL_IID_PLAY, &audioPlayerPlay);
}

SLresult AudioOutput::setAudioPlayerStatePlaying() {
	SLresult result = (*audioPlayerPlay)->SetPlayState(audioPlayerPlay, SL_PLAYSTATE_PLAYING);
	return result;
}

SLresult AudioOutput::setAudioPlayerStatePaused() {
	SLresult result = (*audioPlayerPlay)->SetPlayState(audioPlayerPlay, SL_PLAYSTATE_PAUSED);
	return result;
}
