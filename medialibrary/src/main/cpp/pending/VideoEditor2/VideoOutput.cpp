//
// Created by wjy on 2019/11/29.
//

#include "VideoOutput.h"

VideoOutput::VideoOutput() {
    mGLVertexShader = OUTPUT_VIEW_VERTEX_SHADER;
    mGLFragmentShader = OUTPUT_VIEW_FRAG_SHADER;
}

bool VideoOutput::initOutput(ANativeWindow *window, int screenWidth, int screenHeight,
                             getTextureCallback produceDataCallback, void *ctx, EGLContext context) {
    this->ctx = ctx;
    this->produceDataCallback = produceDataCallback;
    this->screenWidth = screenWidth;
    this->screenHeight = screenHeight;
    this->surfaceWindow = window;
    this->sharedContext = context;
    queue = new MsgQueue("video output msg queue");
    handler = new VideoOuputHandler(this, queue);
    handler->postMessage(new Msg(CREATE_EGL_CONTEXT));
    pthread_create(&threadId, 0, threadStartCallback, this);
    return true;
}

void VideoOutput::onSurfaceCreated(ANativeWindow *window) {
    ALOGI("onSurfaceCreated");
    if (handler) {
        surfaceWindow = window;
        handler->postMessage(new Msg(CREATE_WINDOW_SURFACE));
        handler->postMessage(new Msg(CREATE_EGL_CONTEXT));
    }
}

void VideoOutput::onSurfaceDestroyed() {
    if (handler) {
        handler->postMessage(new Msg(DESTROY_WINDOW_SURFACE));
    }
}

bool VideoOutput::createEGLContext() {
    ALOGI("createEGLContext");
    initEGL(sharedContext);
    renderTexSurface = createWindowSurface(surfaceWindow);
    if (renderTexSurface != NULL) {
        eglMakeCurrent(mEGLDisplay, renderTexSurface, renderTexSurface, mEGLContext);
    }
    initRenderer();
    eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void VideoOutput::createWindowSurface() {
    renderTexSurface = createWindowSurface(surfaceWindow);
    if (renderTexSurface != NULL) {
        eglMakeCurrent(mEGLDisplay, renderTexSurface, renderTexSurface, mEGLContext);
    }
    initRenderer();
}

bool VideoOutput::renderVideo() {
    FrameTexture* texture = NULL;
    produceDataCallback(&texture, ctx, false);
    if (NULL != texture && mIsGLInitialized) {
        eglMakeCurrent(mEGLDisplay, renderTexSurface, renderTexSurface, mEGLContext);
        eglSwapBuffers(mEGLDisplay, renderTexSurface);
    }
}

void VideoOutput::destroyWindowSurface() {
    if (EGL_NO_SURFACE != renderTexSurface) {
        if (mIsGLInitialized) {
            mIsGLInitialized = false;
            glDeleteProgram(mGLProgramId);
        }
        eglDestroySurface(mEGLDisplay, renderTexSurface);
        renderTexSurface = EGL_NO_SURFACE;
        if (NULL != surfaceWindow) {
            ANativeWindow_release(surfaceWindow);
            surfaceWindow = NULL;
        }
    }
}

void VideoOutput::destroyEGLContext() {
    if(EGL_NO_SURFACE != renderTexSurface) {
        eglMakeCurrent(mEGLDisplay, renderTexSurface, renderTexSurface, mEGLContext);
    }
    destroyWindowSurface();
}

void VideoOutput::signalFrameAvailable() {
    if(handler) {
        handler->postMessage(new Msg(RENDER_FRAME));
    }
}

void VideoOutput::stopOutput() {
    ALOGI("enter VideoOutput::stopOutput");
    if (handler) {
        handler->postMessage(
                new Msg(DESTROY_EGL_CONTEXT));
        handler->postMessage(new Msg(MESSAGE_QUEUE_LOOP_QUIT_FLAG));
        pthread_join(threadId, 0);
        if (queue) {
            queue->abort();
            delete queue;
            queue = NULL;
        }

        delete handler;
        handler = NULL;
    }
    ALOGI("leave VideoOutput::stopOutput");
}

bool VideoOutput::initEGL(EGLContext sharedContext) {
    mEGLDisplay = EGL_NO_DISPLAY;

    EGLint numConfigs;
    EGLint width;
    EGLint height;
    const EGLint attribs[] = { EGL_BUFFER_SIZE, 32, EGL_ALPHA_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
    if((mEGLDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        ALOGE("eglGetDisplay error %d", eglGetError());
        return false;
    }
    if (!eglInitialize(mEGLDisplay, 0 , 0)) {
        ALOGE("eglInitialize error %d", eglGetError());
        return false;
    }
    if (!eglChooseConfig(mEGLDisplay, attribs, &mEGLConfig, 1, &numConfigs)) {
        ALOGE("eglChooseConfig error %d", eglGetError());
        return false;
    }
    EGLint  eglContextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    if (!(mEGLContext = eglCreateContext(mEGLDisplay, mEGLConfig, NULL == sharedContext ? EGL_NO_CONTEXT : sharedContext, eglContextAttributes ))) {
        ALOGE("eglCreateContext error %d", eglGetError());
        // release res
        if(EGL_NO_DISPLAY != mEGLDisplay && EGL_NO_CONTEXT != mEGLContext){
            eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(mEGLDisplay, mEGLContext);
            ALOGE("eglDestroyContext");
        }
        mEGLDisplay = EGL_NO_DISPLAY;
        mEGLContext = EGL_NO_CONTEXT;
        return false;
    }
    return true;
}

EGLSurface VideoOutput::createWindowSurface(ANativeWindow *pWindow) {
    EGLSurface surface = NULL;
    EGLint format;
    if (!pWindow) {
        ALOGE("pWindow null");
        return NULL;
    }
    if (!eglGetConfigAttrib(mEGLDisplay, mEGLConfig, EGL_NATIVE_VISUAL_ID, &format)) {
        ALOGE("eglGetConfigAttrib error");
        // release res
        if(EGL_NO_DISPLAY != mEGLDisplay && EGL_NO_CONTEXT != mEGLContext){
            eglMakeCurrent(mEGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(mEGLDisplay, mEGLContext);
            ALOGE("eglDestroyContext");
        }
        mEGLDisplay = EGL_NO_DISPLAY;
        mEGLContext = EGL_NO_CONTEXT;
        return NULL;
    }
    ANativeWindow_setBuffersGeometry(pWindow, 0, 0, format);
    if (!(surface = eglCreateWindowSurface(mEGLDisplay, mEGLConfig, pWindow, 0))) {
        ALOGE("eglCreateWindowSurface error %d", eglGetError());
        return NULL;
    }
    return surface;
}

bool VideoOutput::initRenderer() {
    mGLProgramId = loadProgram(mGLVertexShader, mGLFragmentShader);
    if (!mGLProgramId) {
        ALOGE("%s loadProgram failed", __FUNCTION__);
        return false;
    }

    if (!mGLProgramId) {
        ALOGE("Could not create program.");
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

GLuint VideoOutput::loadProgram(char *pVertexSource, char *pFragmentSource) {
    GLuint programId;
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        ALOGE("vertexShader null");
        return 0;
    }
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!fragmentShader) {
        ALOGE("vertexShader null");
        return 0;
    }
    programId = glCreateProgram();
    if (programId) {
        glAttachShader(programId, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(programId, fragmentShader);
        checkGlError("glAttachShader");
        glLinkProgram(programId);

        GLint linkStatus = GL_FALSE;
        glGetProgramiv(programId, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(programId, bufLength, NULL, buf);
                    ALOGI("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(programId);
            programId = 0;
        }
    }
    return programId;
}

GLuint VideoOutput::loadShader(GLenum shaderType, const char *pSource) {
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
                    ALOGE("%s %S glCompileShader error %d\n %s\n", __FUNCTION__, __LINE__, shaderType, buf);
                    free(buf);
                }
            } else {
                ALOGE("Guessing at GL_INFO_LOG_LENGTH size\n");
                char* buf = (char*) malloc(0x1000);
                if (buf) {
                    glGetShaderInfoLog(shader, 0x1000, NULL, buf);
                    ALOGE("%s %S Could not compile shader %d:\n%s\n",__FUNCTION__, __LINE__, shaderType, buf);
                    free(buf);
                }
            }
            glDeleteShader(shader);
            shader = 0;
        }
    }
    return shader;
}

bool VideoOutput::checkGlError(const char *op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        ALOGE("after %s() glError (0x%x)\n", op, error);
        return true;
    }
    return false;
}

void* VideoOutput::threadStartCallback(void *myself) {
    VideoOutput *output = (VideoOutput*) myself;
    output->processMessage();
    pthread_exit(0);
    return 0;
}

void VideoOutput::processMessage() {
    bool renderingEnabled = true;
    while (renderingEnabled) {
        Msg* msg = NULL;
        if(queue->dequeueMessage(&msg, true) > 0){
            if(MESSAGE_QUEUE_LOOP_QUIT_FLAG == msg->execute()){
                renderingEnabled = false;
            }
            delete msg;
        }
    }
}

