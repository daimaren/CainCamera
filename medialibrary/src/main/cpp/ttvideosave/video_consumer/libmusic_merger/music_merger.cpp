#include "music_merger.h"

#define LOG_TAG "SongStudio MusicMerger"

MusicMerger::MusicMerger() {
#if CONFIG_SUPPORT_AUDIO_EFFECT
	audioEffectProcessor = NULL;
#endif
	audioSampleRate = -1;
	frameNum = 0;
}

MusicMerger::~MusicMerger() {

}

void MusicMerger::initWithAudioEffectProcessor(int audioSampleRate, AudioEffect *audioEffect) {
	this->audioSampleRate = audioSampleRate;
#if CONFIG_SUPPORT_AUDIO_EFFECT
	if (NULL == audioEffectProcessor) {
		audioEffectProcessor = AudioEffectProcessorFactory::GetInstance()->buildLiveAudioEffectProcessor();
		audioEffectProcessor->init(audioEffect);
	}
#endif
}

/** 在直播中调用 **/
#if CONFIG_SUPPORT_AUDIO_EFFECT
int MusicMerger::mixtureMusicBuffer(long frameNum, short* accompanySamples, int accompanySampleSize, short* audioSamples, int audioSampleSize) {
	int actualSize = min(accompanySampleSize, audioSampleSize);
	//通过frameNum计算出position（单位是ms）
	float position = frameNum * 1000 / audioSampleRate;
	AudioResponse* response = audioEffectProcessor->process(audioSamples, audioSampleSize, accompanySamples, accompanySampleSize, position, frameNum);
	// 经过sound touch处理返回的实际大小
	int *soundTouchReceiveSamples = (int *)response->get(PITCH_SHIFT_MIX_RESPONSE_KEY_RECEIVE_SAMPLES_SIZE);
	if (soundTouchReceiveSamples != NULL) {
		audioSampleSize = *soundTouchReceiveSamples;
		delete soundTouchReceiveSamples;
	}
	return actualSize;
}
#else
int MusicMerger::mixtureMusicBuffer(long frameNum, short* accompanySamples, int accompanySampleSize, short* audioSamples, int audioSampleSize) {
	for(int i = audioSampleSize - 1; i >= 0; i--) {
		audioSamples[i] = audioSamples[i / 2];
	}
	int actualSize = MIN(accompanySampleSize, audioSampleSize);
	LOGI("accompanySampleSize is %d audioSampleSize is %d", accompanySampleSize, audioSampleSize);
	mixtureAccompanyAudio(accompanySamples, audioSamples, actualSize, audioSamples);
	return actualSize;
}
#endif

void MusicMerger::setAudioEffect(AudioEffect *audioEffect) {
#if CONFIG_SUPPORT_AUDIO_EFFECT
	if(NULL != audioEffectProcessor){
		audioEffectProcessor->setAudioEffect(audioEffect);
	}
#endif
}

void MusicMerger::stop() {
#if CONFIG_SUPPORT_AUDIO_EFFECT
	if (NULL != audioEffectProcessor) {
		audioEffectProcessor->destroy();
		delete audioEffectProcessor;
		audioEffectProcessor = NULL;
	}
#endif
}
