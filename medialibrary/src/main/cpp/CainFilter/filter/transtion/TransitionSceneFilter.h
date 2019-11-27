//
// Created by 1 on 2019/11/26.
//

#ifndef CAINCAMERA_TRANSITIONSCENEFILTER_H
#define CAINCAMERA_TRANSITIONSCENEFILTER_H


#include "../GLFilter.h"
#include <AndroidLog.h>
#include <GLES2/gl2.h>

class TransitionSceneFilter : public GLFilter{

public:
    void initProgram(const char *vertexShader, const char *fragmentShader) override;

private:
    //private funcions
    void checkGlError(const char* op);
    void drawTexture(GLuint input_tex, GLuint output_tex, const float *vertices, const float *textureVertices,
                               bool viewPortUpdate);
private:
    GLuint mdstSampler;
    GLint mProgress;
    GLint mDstTexCoord;


};


#endif //CAINCAMERA_TRANSITIONSCENEFILTER_H
