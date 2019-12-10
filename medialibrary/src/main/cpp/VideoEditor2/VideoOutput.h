//
// Created by wjy on 2019/11/29.
//

#ifndef CAINCAMERA_VIDEOOUTPUT_H
#define CAINCAMERA_VIDEOOUTPUT_H
#include <android/native_window.h>
#include <EGL/egl.h>
#include "MsgQueue.h"
#include "Handler.h"
#include "utils.h"
#include "circle_texture_queue.h"

static char* OUTPUT_VIEW_VERTEX_SHADER =
        "attribute vec4 position;    \n"
        "attribute vec2 texcoord;   \n"
        "varying vec2 v_texcoord;     \n"
        "void main(void)               \n"
        "{                            \n"
        "   gl_Position = position;  \n"
        "   v_texcoord = texcoord;  \n"
        "}                            \n";

static char* OUTPUT_VIEW_FRAG_SHADER =
        "varying highp vec2 v_texcoord;\n"
        "uniform sampler2D yuvTexSampler;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(yuvTexSampler, v_texcoord);\n"
        "}\n";
typedef enum {
    CREATE_EGL_CONTEXT,
    CREATE_WINDOW_SURFACE,
    DESTROY_WINDOW_SURFACE,
    DESTROY_EGL_CONTEXT,
    RENDER_FRAME
} VideoOutputMSGType;

typedef int (*getTextureCallback)(FrameTexture** tex, void* ctx, bool forceGetFrame);

class VideoOuputHandler;
class VideoOutput {
public:
    VideoOutput();
    virtual ~VideoOutput(){};
    bool initOutput(ANativeWindow* window, int screenWidth, int screenHeight, getTextureCallback produceDataCallback,
                    void* ctx, EGLContext context);
    void signalFrameAvailable();
    void onSurfaceCreated(ANativeWindow* window);
    void onSurfaceDestroyed();

    bool createEGLContext();
    void createWindowSurface();
    bool renderVideo();
    void destroyWindowSurface();
    void destroyEGLContext();
    void stopOutput();
    bool initEGL(EGLContext sharedContext);
    EGLSurface createWindowSurface(ANativeWindow *pWindow);
    bool initRenderer();
    GLuint loadProgram(char *pVertexSource, char *pFragmentSource);
    GLuint loadShader(GLenum shaderType, const char *pSource);
    bool checkGlError(const char *op);
private:
    static void* threadStartCallback(void *myself);
    void processMessage();
protected:
    getTextureCallback produceDataCallback;
    void* ctx;
    VideoOuputHandler* handler;
    MsgQueue* queue;
    //EGL
    EGLDisplay mEGLDisplay;
    EGLConfig mEGLConfig;
    EGLContext mEGLContext;
    EGLSurface renderTexSurface;
    EGLContext sharedContext;
    ANativeWindow* surfaceWindow;
    int screenWidth;
    int screenHeight;
    //OpenGL Render
    char* mGLVertexShader;
    char* mGLFragmentShader;
    GLuint mGLProgramId;
    GLuint mGLVertexCoords;
    GLuint mGLTextureCoords;
    GLint mGLUniformTexture;
    bool mIsGLInitialized;

    pthread_t threadId;
};

class VideoOuputHandler : public Handler {
public:
    VideoOuputHandler(VideoOutput* videoOutput, MsgQueue* queue)
            :Handler(queue) {
        this->videoOutput = videoOutput;
        initPlayerResourceFlag = false;
    }
    void handleMessage(Msg* msg) {
        int what = msg->getWhat();
        switch (what) {
            case CREATE_EGL_CONTEXT:
                initPlayerResourceFlag = videoOutput->createEGLContext();
                break;
            case RENDER_FRAME:
                if(initPlayerResourceFlag){
                    videoOutput->renderVideo();
                }
                break;
            case CREATE_WINDOW_SURFACE:
                if(initPlayerResourceFlag){
                    videoOutput->createWindowSurface();
                }
                break;
            case DESTROY_WINDOW_SURFACE:
                if(initPlayerResourceFlag){
                    videoOutput->destroyWindowSurface();
                }
                break;
            case DESTROY_EGL_CONTEXT:
                videoOutput->destroyEGLContext();
                break;
        }
    }
private:
    VideoOutput* videoOutput;
    bool initPlayerResourceFlag;
};
#endif //CAINCAMERA_VIDEOOUTPUT_H
