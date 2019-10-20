//
// Created by CainHuang on 2019/8/17.
//
#include "unistd.h"
#include "MediaRecorder.h"

/**
 * 主要到事情说三遍，为了吃透核心代码，先不要做封装，所有核心代码写在这一个cpp里
 * 第一阶段，先把相机画面显示出来
 */
MediaRecorder::MediaRecorder() : mRecordListener(nullptr), mAbortRequest(true),
                                     mStartRequest(false), mExit(true), mRecordThread(nullptr),
                                     mYuvConvertor(nullptr), mFrameQueue(nullptr){
    mRecordParams = new RecordParams();
    mVertexShader = OUTPUT_VIEW_VERTEX_SHADER;
    mFragmentShader = OUTPUT_VIEW_FRAG_SHADER;
}

MediaRecorder::~MediaRecorder() {
    release();
    if (mRecordParams != nullptr) {
        delete mRecordParams;
        mRecordParams = nullptr;
    }
}
/**
 * prepareEGLContext
 */

void MediaRecorder::prepareEGLContext(ANativeWindow *window, JavaVM *g_jvm, jobject obj,
                                      int screenWidth, int screenHeight, int cameraFacingId) {
    aw_log("prepareEGLContext");
    mJvm = g_jvm;
    mObj = obj;
    mScreenWidth = screenWidth;
    mScreenHeight = screenHeight;
    mFacingId = cameraFacingId;
    initEGL();
    createWindowSurface(window);
    if (mPreviewSurface != NULL) {
        eglMakeCurrent(mEGLDisplay, mPreviewSurface, mPreviewSurface, mEGLContext);
    }
    mTextureWidth = 360; //720
    mTextureHeight = 640; //1280
    aw_log("camera : {%d, %d}", mScreenWidth, mScreenHeight);
    aw_log("Texture : {%d, %d}", mTextureWidth, mTextureHeight);
    initRenderer();
    int ret = initTexture();
    if (ret < 0) {
        LOGI("init texture failed");
        if (mDecodeTexId) {
            glDeleteTextures(1, &mDecodeTexId);
        }
    }
    //todo init inputTexId outputTexId
    startCameraPreview();
}

void MediaRecorder::notifyFrameAvailable() {
    updateTexImage();
    // todo processVideoFrame
    if (mPreviewSurface != EGL_NO_SURFACE) {
        eglMakeCurrent(mEGLDisplay, mPreviewSurface, mPreviewSurface, mEGLContext);
        renderToViewWithAutofit(mDecodeTexId, mScreenWidth, mScreenHeight, mTextureWidth, mTextureHeight);
        eglSwapBuffers(mEGLDisplay, mPreviewSurface);
    }
    if (isEncoding) {
        //todo encode, pending
    }
}

/**
 * 设置录制监听器
 * @param listener
 */
void MediaRecorder::setOnRecordListener(OnRecordListener *listener) {
    mRecordListener = listener;
}

/**
 * 释放资源
 */
void MediaRecorder::release() {
    stopRecord();
    // 等待退出
    mMutex.lock();
    while (!mExit) {
        mCondition.wait(mMutex);
    }
    mMutex.unlock();

    if (mRecordListener != nullptr) {
        delete mRecordListener;
        mRecordListener = nullptr;
    }
    if (mFrameQueue != nullptr) {
        delete mFrameQueue;
        mFrameQueue = nullptr;
    }
    if (mRecordThread != nullptr) {
        delete mRecordThread;
        mRecordThread = nullptr;
    }
    //update duration and file size
    if (mFile == nullptr) {
        aw_log("mFile nullptr");
        return;
    }

    int64_t  file_size = ftello(mFile);
    aw_data* flv_data = alloc_aw_data(30);

    //写入duration 0表示double，1表示uint8
    data_writer.write_string(&flv_data, "duration", 2);
    data_writer.write_uint8(&flv_data, 0);
    data_writer.write_double(&flv_data, duration);
    //写入file_size
    data_writer.write_string(&flv_data, "filesize", 2);
    data_writer.write_uint8(&flv_data, 0);
    data_writer.write_double(&flv_data, file_size);

    if (mFile) {
        fseek(mFile, 42, SEEK_SET);

        size_t write_item_count = fwrite(flv_data->data, 1, flv_data->size, mFile);
        fclose(mFile);
    }
    aw_sw_encoder_close_faac_encoder();
    aw_sw_encoder_close_x264_encoder();
}

RecordParams* MediaRecorder::getRecordParams() {
    return mRecordParams;
}

/**
 * 初始化录制器
 * @param params
 * @return
 */
int MediaRecorder::prepare() {

    RecordParams *params = mRecordParams;
    if (params->rotateDegree % 90 != 0) {
        LOGE("invalid rotate degree: %d", params->rotateDegree);
        return -1;
    }

    int ret;

    mFrameQueue = new SafetyQueue<AVMediaData *>();

    LOGI("Record to file: %s, width: %d, height: %d", params->dstFile, params->width,
         params->height);

    // yuv转换
    int outputWidth = params->width;
    int outputHeight = params->height;

    // yuv转换器
    mYuvConvertor = new YuvConvertor();
    mYuvConvertor->setInputParams(params->width, params->height, params->pixelFormat);
    mYuvConvertor->setCrop(params->cropX, params->cropY, params->cropWidth, params->cropHeight);
    mYuvConvertor->setRotate(params->rotateDegree);
    mYuvConvertor->setScale(params->scaleWidth, params->scaleHeight);
    mYuvConvertor->setMirror(params->mirror);
    // 准备
    if (mYuvConvertor->prepare() < 0) {
        delete mYuvConvertor;
        mYuvConvertor = nullptr;
    } else {
        outputWidth = mYuvConvertor->getOutputWidth();
        outputHeight = mYuvConvertor->getOutputHeight();
        if (outputWidth == 0 || outputHeight == 0) {
            outputWidth = params->rotateDegree % 180 == 0 ? params->width : params->height;
            outputHeight = params->rotateDegree % 180 == 0 ? params->height : params->width;
        }
    }

    // 准备好A/V编码器, 复用器
    aw_faac_config faacConfig;
    faacConfig.sample_rate = params->sampleRate;
    faacConfig.bitrate = 100000;
    faacConfig.channel_count = params->channels;
    faacConfig.sample_size = params->sampleFormat;

    aw_sw_encoder_open_faac_encoder(&faacConfig);

    aw_x264_config *pX264_config = alloc_aw_x264_config();
    pX264_config->width = outputWidth;
    pX264_config->height = outputHeight;
    pX264_config->bitrate = 1000000;
    pX264_config->fps = params->frameRate;
    pX264_config->input_data_format = getX264PixelFormat((PixelFormat)params->pixelFormat);

    aw_sw_encoder_open_x264_encoder(pX264_config);

    // write FLV Header
    aw_data *awData = alloc_aw_data(13);
    aw_write_flv_header(&awData);
    aw_write_data_to_file(params->dstFile, awData);

    usleep(1000);
    mFile = fopen(params->dstFile, "wb");

    // write Metadata Tag
    aw_flv_script_tag *script_tag = alloc_aw_flv_script_tag();

    script_tag->duration = 0;
    script_tag->width = outputWidth;
    script_tag->height = outputHeight;
    script_tag->video_data_rate = 1000;
    script_tag->frame_rate = params->frameRate;

    script_tag->a_sample_rate = params->sampleRate;
    script_tag->a_sample_size = params->sampleFormat;
    if (params->channels == 1) {
        script_tag->stereo = 0;
    } else {
        script_tag->stereo = 1;
    }
    script_tag->file_size = 0;
    if(script_tag) {
        save_script_data(script_tag);
        LOGD("save Metadata Tag success");
    }else{
        LOGE("pScriptTag null pointer");
    }
    aw_log("prepare done");
    return 0;
}

/**
 * 录制一帧数据
 * @param data
 * @return
 */
int MediaRecorder::recordFrame(AVMediaData *data) {
    if (mAbortRequest || mExit) {
        LOGE("Recoder is not recording.");
        delete data;
        return -1;
    }

    // 录制的是音频数据并且不允许音频录制，则直接删除
    if (!mRecordParams->enableAudio && data->getType() == MediaAudio) {
        delete data;
        return -1;
    }

    // 录制的是视频数据并且不允许视频录制，直接删除
    if (!mRecordParams->enableVideo && data->getType() == MediaVideo) {
        delete data;
        return -1;
    }

    // 将媒体数据入队
    if (mFrameQueue != nullptr) {
        mFrameQueue->push(data);
    } else {
        delete data;
    }
    return 0;
}

/**
 * 开始录制
 */
void MediaRecorder::startRecord() {
    mMutex.lock();
    mAbortRequest = false;
    mStartRequest = true;
    mCondition.signal();
    mMutex.unlock();

    if (mRecordThread == nullptr) {
        mRecordThread = new Thread(this);
        mRecordThread->start();
        mRecordThread->detach();
    }
}

/**
 * 停止录制
 */
void MediaRecorder::stopRecord() {
    mMutex.lock();
    mAbortRequest = true;
    mCondition.signal();
    mMutex.unlock();
    if (mRecordThread != nullptr) {
        mRecordThread->join();
        delete mRecordThread;
        mRecordThread = nullptr;
    }
}

/**
 * 判断是否正在录制
 * @return
 */
bool MediaRecorder::isRecording() {
    bool recording = false;
    mMutex.lock();
    recording = !mAbortRequest && mStartRequest && !mExit;
    mMutex.unlock();
    return recording;
}

void MediaRecorder::run() {
    int ret = 0;
    int64_t start = 0;
    int64_t current = 0;
    mExit = false;

    // 录制回调监听器
    if (mRecordListener != nullptr) {
        mRecordListener->onRecordStart();
    }

    LOGD("waiting to start record");
    while (!mStartRequest) {
        if (mAbortRequest) { // 停止请求则直接退出
            break;
        } else { // 睡眠10毫秒继续
            usleep(10 * 1000);
        }
    }

    // 开始录制编码流程
    if (!mAbortRequest && mStartRequest) {
        LOGD("start record");
        // 正在运行，并等待frameQueue消耗完
        while (!mAbortRequest || !mFrameQueue->empty()) {
            if (!mFrameQueue->empty()) {

                // 从帧对列里面取出媒体数据
                auto data = mFrameQueue->pop();
                if (!data) {
                    continue;
                }
                if (start == 0) {
                    start = data->getPts();
                }
                if (data->getPts() >= current) {
                    current = data->getPts();
                }

                // yuv转码
                if (data->getType() == MediaVideo && mYuvConvertor != nullptr) {
                    // 将数据转换成Yuv数据，处理失败则开始处理下一帧
                    if (mYuvConvertor->convert(data) < 0) {
                        LOGE("Failed to convert video data to yuv420");
                        delete data;
                        continue;
                    }
                }
                if (data->getType() == MediaVideo) {
                    // x264编码
                    aw_flv_video_tag *video_tag = aw_sw_encoder_encode_x264_data((int8_t*)data->image, data->length, data->pts);
                    if (video_tag) {
                        saveFlvVideoTag(video_tag);
                    }
                } else if (data->getType() == MediaAudio) {
                    // faac编码
                    int timestamp = aw_sw_faac_encoder_max_input_sample_count() * 1000 / mRecordParams->sampleRate;
                    aw_flv_audio_tag *audio_tag = aw_sw_encoder_encode_faac_data((int8_t *)data->sample, data->sample_size, timestamp);
                    if (audio_tag) {
                        saveFlvAudioTag(audio_tag);
                    }
                }

                if (ret < 0) {
                    LOGE("Failed to encoder media data： %s", data->getName());
                } else {
                    LOGD("recording time: %f", (float)(current - start));
                    if (mRecordListener != nullptr) {
                        mRecordListener->onRecording((float)(current - start));
                    }
                }
                // 释放资源
                delete data;
            }
        }
    }

    // 通知退出成功
    mExit = true;
    mCondition.signal();

    // 录制完成回调
    if (mRecordListener != nullptr) {
        mRecordListener->onRecordFinish(ret == 0, (float)(current - start));
    }
    duration = current - start;
}

/**
 * Save Flv AudioTag
 */
void MediaRecorder::saveFlvAudioTag(aw_flv_audio_tag *audio_tag) {
    if(!audio_tag){
        LOGE("audio_tag null pointer");
        return;;
    }
    if(!isSpsPpsAndAudioSpecificConfigSent){
        saveSpsPpsAndAudioSpecificConfigTag();
        free_aw_flv_audio_tag(&audio_tag);
    } else{
        save_audio_data(audio_tag);
    }
}

/**
 * Save Flv VideoTag
 */
void MediaRecorder::saveFlvVideoTag(aw_flv_video_tag *video_tag) {

    if(!video_tag){
        LOGE("video_tag null pointer");
        return;;
    }
    if(!isSpsPpsAndAudioSpecificConfigSent){
        saveSpsPpsAndAudioSpecificConfigTag();
        free_aw_flv_video_tag(&video_tag);
    } else{
        save_video_data(video_tag);
    }
}

/**
 * 根据flv，h264，aac协议，提供首帧需要发送的tag
 */
void MediaRecorder::saveSpsPpsAndAudioSpecificConfigTag() {
    aw_flv_video_tag *spsPpsTag = NULL;
    aw_flv_audio_tag *audioSpecificConfigTag = NULL;
    if(isSpsPpsAndAudioSpecificConfigSent){
        LOGE("already send sps pps or not capture");
        return;
    }
    //create video sps pps tag
    spsPpsTag = aw_sw_encoder_create_x264_sps_pps_tag();
    if(spsPpsTag) {
        save_video_data(spsPpsTag);
        LOGD("save sps pps success");
    }else{
        LOGE("spsPpsTag null pointer");
    }

    //audio specific config tag
    audioSpecificConfigTag = aw_sw_encoder_create_faac_specific_config_tag();
    if(audioSpecificConfigTag){
        save_audio_data(audioSpecificConfigTag);
        LOGD("save audio Specific Tag success");
    }else{
        LOGE("audioSpecificConfigTag null pointer");
    }

    isSpsPpsAndAudioSpecificConfigSent = spsPpsTag || audioSpecificConfigTag;
    LOGD("is sps pps and audio sepcific config sent=%d", isSpsPpsAndAudioSpecificConfigSent);
}

void MediaRecorder::save_audio_data(aw_flv_audio_tag *audio_tag){
    save_flv_tag_to_file(&audio_tag->common_tag);
}

void MediaRecorder::save_video_data(aw_flv_video_tag *video_tag) {
    save_flv_tag_to_file(&video_tag->common_tag);
}

void MediaRecorder::save_script_data(aw_flv_script_tag *script_tag) {
    save_flv_tag_to_file(&script_tag->common_tag);
}

void MediaRecorder::save_flv_tag_to_file(aw_flv_common_tag *commonTag) {
    if (commonTag) {
        aw_write_flv_tag(&s_output_buf, commonTag);
        switch (commonTag->tag_type) {
            case aw_flv_tag_type_audio: {
                free_aw_flv_audio_tag(&commonTag->audio_tag);
                break;
            }
            case aw_flv_tag_type_video: {
                free_aw_flv_video_tag(&commonTag->video_tag);
                break;
            }
            case aw_flv_tag_type_script: {
                free_aw_flv_script_tag(&commonTag->script_tag);
                break;
            }
        }
    }

    if (s_output_buf->size <= 0) {
        return;
    }

    aw_data *data = alloc_aw_data(s_output_buf->size);
    memcpy(data->data, s_output_buf->data, s_output_buf->size);
    data->size = s_output_buf->size;
    if (mFile) {
        size_t count = fwrite(data->data, 1, data->size, mFile);
        aw_log("save flv tag size=%d", count);
    }
    reset_aw_data(&s_output_buf);
}

bool MediaRecorder::initEGL() {
    mEGLDisplay = EGL_NO_DISPLAY;
    mEGLContext = EGL_NO_CONTEXT;

    EGLint numConfigs;
    EGLint width;
    EGLint height;
    const EGLint attribs[] = { EGL_BUFFER_SIZE, 32, EGL_ALPHA_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
    if((mEGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        aw_log("eglGetDisplay error %d", eglGetError());
        return false;
    }
    if (!eglInitialize(mEGLDisplay, 0 , 0)) {
        aw_log("eglInitialize error %d", eglGetError());
        return false;
    }
    if (!eglChooseConfig(mEGLDisplay, attribs, &mEGLConfig, 1, &numConfigs)) {
        aw_log("eglChooseConfig error %d", eglGetError());
        return false;
    }
    EGLint  eglContextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    if (!(mEGLContext = eglCreateContext(mEGLDisplay, mEGLConfig, EGL_NO_CONTEXT, eglContextAttributes ))) {
        aw_log("eglCreateContext error %d", eglGetError());
        // release res
        if(EGL_NO_DISPLAY != mEGLDisplay && EGL_NO_CONTEXT != mEGLContext){
            eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(mEGLDisplay, mEGLContext);
            aw_log("eglDestroyContext");
        }
        mEGLDisplay = EGL_NO_DISPLAY;
        mEGLContext = EGL_NO_CONTEXT;
        return false;
    }
    return true;
}

bool MediaRecorder::createWindowSurface(ANativeWindow *pWindow) {
    EGLint format;
    if (!pWindow) {
        aw_log("pWindow null");
        return false;
    }
    if (!eglGetConfigAttrib(mEGLDisplay, mEGLConfig, EGL_NATIVE_VISUAL_ID, &format)) {
        aw_log("eglGetConfigAttrib error");
        // release res
        if(EGL_NO_DISPLAY != mEGLDisplay && EGL_NO_CONTEXT != mEGLContext){
            eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(mEGLDisplay, mEGLContext);
            aw_log("eglDestroyContext");
        }
        mEGLDisplay = EGL_NO_DISPLAY;
        mEGLContext = EGL_NO_CONTEXT;
        return false;
    }
    ANativeWindow_setBuffersGeometry(pWindow, 0, 0, format);
    if (!(mPreviewSurface = eglCreateWindowSurface(mEGLDisplay, mEGLConfig, pWindow, 0))) {
        aw_log("eglCreateWindowSurface error %d", eglGetError());
        return false;
    }
    return true;
}

void MediaRecorder::startCameraPreview() {
    aw_log("startCameraPreview");
    JNIEnv *env;
    if (mJvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
        aw_log("%s AttachCurrentThread failed", __FUNCTION__);
        return;
    }
    if (env == NULL) {
        aw_log("%s getJNIEnv failed", __FUNCTION__);
        return;
    }
    jclass jcls = env->GetObjectClass(mObj);
    if (NULL != jcls) {
        jmethodID  startPreviewCallback = env->GetMethodID(jcls, "startPreviewFromNative", "(I)V");
        if (startPreviewCallback != NULL) {
            env->CallVoidMethod(mObj, startPreviewCallback, mDecodeTexId);
        }
    }
    if (mJvm->DetachCurrentThread() != JNI_OK) {
        LOGE("%s %s DetachCurrentThread failed", __FUNCTION__, __LINE__);
        return;
    }
}

bool MediaRecorder::initRenderer() {
    mGLProgramId = loadProgram(mVertexShader, mFragmentShader);
    if (!mGLProgramId) {
        aw_log("%s loadProgram failed", __FUNCTION__);
        return false;
    }

    if (!mGLProgramId) {
        LOGE("Could not create program.");
        return false;
    }
    mGLVertexCoords = glGetAttribLocation(mGLProgramId, "position");
    checkGlError("glGetAttribLocation vPosition");
    mGLTextureCoords = glGetAttribLocation(mGLProgramId, "texcoord");
    checkGlError("glGetAttribLocation vTexCords");
    mGLUniformTexture = glGetUniformLocation(mGLProgramId, "yuvTexSampler");
    checkGlError("glGetAttribLocation yuvTexSampler");
    mIsGLInitialized = true;
    return true;
}

GLuint MediaRecorder::loadProgram(char *pVertexSource, char *pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        aw_log("vertexShader null");
        return 0;
    }
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!fragmentShader) {
        aw_log("vertexShader null");
        return 0;
    }
    mGLProgramId = glCreateProgram();
    if (mGLProgramId) {
        glAttachShader(mGLProgramId, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(mGLProgramId, fragmentShader);
        checkGlError("glAttachShader");
        glLinkProgram(mGLProgramId);

        GLint linkStatus = GL_FALSE;
        glGetProgramiv(mGLProgramId, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(mGLProgramId, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(mGLProgramId, bufLength, NULL, buf);
                    LOGI("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(mGLProgramId);
            mGLProgramId = 0;
        }
    }
}

GLuint MediaRecorder::loadShader(GLenum shaderType, const char *pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*)malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    aw_log("%s %S glCompileShader error %d\n %s\n", __FUNCTION__, __LINE__, shaderType, buf);
                    free(buf);
                }
            } else {
                aw_log("Guessing at GL_INFO_LOG_LENGTH size\n");
                char* buf = (char*) malloc(0x1000);
                if (buf) {
                    glGetShaderInfoLog(shader, 0x1000, NULL, buf);
                    LOGI("%s %S Could not compile shader %d:\n%s\n",__FUNCTION__, __LINE__, shaderType, buf);
                    free(buf);
                }
            }
            glDeleteShader(shader);
            shader = 0;
        }
    }
    return shader;
}

bool MediaRecorder::checkGlError(const char *op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        LOGE("after %s() glError (0x%x)\n", op, error);
        return true;
    }
    return false;
}

int MediaRecorder::initTexture() {
    mDecodeTexId = 0;
    glGenTextures(1, &mDecodeTexId);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, mDecodeTexId);
    if (checkGlError("glBindTexture")) {
        return -1;
    }
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (checkGlError("glTexParameteri")) {
        return -1;
    }
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    if (checkGlError("glTexParameteri")) {
        return -1;
    }
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    if (checkGlError("glTexParameteri")) {
        return -1;
    }
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (checkGlError("glTexParameteri")) {
        return -1;
    }
    return 1;
}

void MediaRecorder::renderToViewWithAutofit(GLuint texId, int screenWidth, int screenHeight, int texWidth, int texHeight) {
    glViewport(0, 0, screenWidth, screenHeight);

    if (mIsGLInitialized) {
        LOGE("render not init");
        return;
    }

    int fitHeight = (int) ((float) texHeight * screenWidth / texWidth + 0.5f);
    float cropRatio = (float) (fitHeight - screenHeight) / (2 * fitHeight);

    if (cropRatio < 0) {
        cropRatio = 0.0f;
    }

    glUseProgram(mGLProgramId);
    static const GLfloat _vertices[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f,
                                        1.0f, 1.0f};
    glVertexAttribPointer(mGLVertexCoords, 2, GL_FLOAT, 0, 0, _vertices);
    glEnableVertexAttribArray(mGLVertexCoords);
    static const GLfloat texCoords[] = {0.0f, cropRatio, 1.0f, cropRatio, 0.0f, 1.0f - cropRatio,
                                        1.0f, 1.0f - cropRatio};
    glVertexAttribPointer(mGLTextureCoords, 2, GL_FLOAT, 0, 0, texCoords);
    glEnableVertexAttribArray(mGLTextureCoords);
    //bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texId);
    glUniform1i(mGLUniformTexture, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(mGLVertexCoords);
    glDisableVertexAttribArray(mGLTextureCoords);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
}

void MediaRecorder::updateTexImage() {
    JNIEnv *env;
    if (mJvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
        LOGE("%s: AttachCurrentThread() failed", __FUNCTION__);
        return;
    }
    if (env == NULL) {
        LOGI("getJNIEnv failed");
        return;
    }
    jclass jcls = env->GetObjectClass(mObj);
    if (NULL != jcls) {
        jmethodID updateTexImageCallback = env->GetMethodID(jcls, "updateTexImageFromNative",
                                                            "()V");
        if (NULL != updateTexImageCallback) {
            env->CallVoidMethod(mObj, updateTexImageCallback);
        }
    }
    if (mJvm->DetachCurrentThread() != JNI_OK) {
        LOGE("%s: DetachCurrentThread() failed", __FUNCTION__);
    }
}















