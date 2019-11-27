//
// Created by 1 on 2019/11/26.
//

#include "TransitionSceneFilter.h"

//sampler2D代表是2D纹理对象
//uniform代表是所有的片元着色器变量值是一样到
//texture2D函数作用是根据纹理坐标，采样获取纹理的像素
const std::string kTransitionSceneFragmentShader = SHADER_TO_STRING(
        precision lowp float;
        varying vec2 srcTexCoord;
        varying vec2 dstTexCoord;
        uniform sampler2D inputTexture;
        uniform sampler2D dstSampler;
        uniform float progress;
        void main()
        {
            lowp vec4 srcColor = texture2D(yuvTexSampler, srcTexCoord);
            lowp vec4 dstColor = texture2D(dstSampler, dstTexCoord);
            gl_FragColor = mix(srcColor, dstColor, progress);
        }
);
//attribute关键词代表是顶点着色器输入变量
//varying关键词代表是从顶点着色器向片元着色器传递数据到变量
const std::string kTransitionSceneVertexShader = SHADER_TO_STRING(
        attribute vec4 aPosition;
        attribute vec2 aTextureCoord;
        attribute vec2 dstTexCoordAttr;
        varying highp vec2 srcTexCoord;
        varying highp vec2 dstTexCoord;
        void main(void)
        {
           srcTexCoord = texcoord;
           dstTexCoord = dstTexCoordAttr;
           gl_Position = position;
        }
);

void TransitionSceneFilter::checkGlError(const char *op) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        ALOGE("after %s() glError (0x%x)\n", op, error);
    }
}

void TransitionSceneFilter::initProgram(const char *vertexShader, const char *fragmentShader) {
    GLFilter::initProgram(kTransitionSceneVertexShader.c_str(), kTransitionSceneFragmentShader.c_str());
    if (isInitialized()) {
        mDstTexCoord = glGetAttribLocation(programHandle, "dstTexCoordAttr");
        checkGlError("glGetAttribLocation dstTexCoordAttr");
        mdstSampler = glGetUniformLocation(programHandle, "dstSampler");
        checkGlError("glGetUniformLocation dstSampler");
        mProgress = glGetUniformLocation(programHandle, "progress");
        checkGlError("glGetUniformLocation progress");
    }
}

void TransitionSceneFilter::drawTexture(GLuint input_tex, GLuint output_tex, const float *vertices,
                                        const float *textureVertices, bool viewPortUpdate) {

}


