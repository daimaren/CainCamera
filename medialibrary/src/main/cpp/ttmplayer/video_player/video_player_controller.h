#ifndef VIDEO_PLAYER_CONTROLLER_H
#define VIDEO_PLAYER_CONTROLLER_H

#include "CommonTools.h"
#include <queue>
#include <unistd.h>
#include <pthread.h>
#include <android/native_window.h>
#include "./sync/av_synchronizer.h"
#include "./audio_output.h"
#include "opengl_media/render/video_gl_surface_render.h"
#include "./video_output.h"
#include "egl_core/egl_share_context.h"
#include "../android/AVMessageQueue.h"
/**
 * Video Player Controller
 */
class MediaPlayer {
public:
	MediaPlayer();
	virtual ~MediaPlayer();

	status_t prepare(char *srcFilenameParam, int* max_analyze_duration, int analyzeCnt, int probesize, bool fpsProbeSizeConfigured, float minBufferedDuration, float maxBufferedDuration);

	void start();

	void pause();

	void resume();

	void seekTo(float position);

	int isPlaying();

	int isLooping();

	float getDuration(); //单位秒, 保留三位小数相当于精度到毫秒

	int getVideoFrameWidth();

	int getVideoFrameHeight();

	float getBufferedProgress();

	float getCurrentPosition();

	virtual void destroy();

	void changeFilter(int type, const char *name);

	void changeFilter(int type, const int id);

	void beginFilter(int type, const char *name);

	void endFilter(int type, const char *name);

	void startEncoding(int width, int height, int videoBitRate, int frameRate,
					   int useHardWareEncoding,int strategy);
	void stopEncoding();

	/** 重置播放区域的大小,比如横屏或者根据视频的ratio来调整 **/
	void resetRenderSize(int left, int top, int width, int height);
	/** 关键的回调方法 **/
	//当音频播放器播放完毕一段buffer之后，会回调这个方法，这个方法要做的就是用数据将这个buffer再填充起来
	static int audioCallbackFillData(byte* buffer, size_t bufferSize, void* ctx);
	int consumeAudioFrames(byte* outData, size_t bufferSize);
	//当视频播放器接受到要播放视频的时候，会回调这个方法，这个方法
	static int videoCallbackGetTex(FrameTexture** frameTex, void* ctx, bool forceGetFrame);
	virtual int getCorrectRenderTexture(FrameTexture** frameTex, bool forceGetFrame);

	virtual bool startAVSynchronizer();

	void onSurfaceCreated(ANativeWindow* window, int widht, int height);
	void onSurfaceDestroyed();

	void signalOutputFrameAvailable();

	EGLContext getUploaderEGLContext();
    AVMessageQueue* getMessageQueue();
public:
    AVMessageQueue *messageQueue;
protected:
	ANativeWindow* window;
	int screenWidth;
	int screenHeight;

	EGLContext mSharedEGLContext;

	bool mIsPlaying;
	DecoderRequestHeader* requestHeader;
	float minBufferedDuration;
	float maxBufferedDuration;

	/** 用于回调Java层 **/
	JavaVM *g_jvm;
	jobject obj;

	AVSynchronizer* synchronizer;
	VideoOutput* videoOutput;
	AudioOutput* audioOutput;
	bool initAudioOutput();
	virtual int getAudioChannels();

	virtual bool initAVSynchronizer();

	bool userCancelled; //用户取消拉流
	pthread_t initThreadThreadId;

	static void* initThreadCallback(void *myself);
	void initVideoOutput(ANativeWindow* window);
};
#endif //VIDEO_PLAYER_CONTROLLER_H
