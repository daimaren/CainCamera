//
// Created by 1 on 2019/12/12.
//

#include <common/OpenGLUtils.h>
#include "GLCommonFrameFilter.h"

const std::string kCommonFrameFragmentShader = SHADER_TO_STRING(
        precision highp float;

        uniform sampler2D inputTexture;
        varying vec2 textureCoordinate;

        uniform float horizontal;  // (1)
        uniform float vertical;

        void main (void) {
            float horizontalCount = max(horizontal, 1.0);  // (2)
            float verticalCount = max(vertical, 1.0);

            float ratio = verticalCount / horizontalCount;  // (3)

            vec2 originSize = vec2(1.0, 1.0);
            vec2 newSize = originSize;

            if (ratio > 1.0) {
                newSize.y = 1.0 / ratio;
            } else {
                newSize.x = ratio;
            }

            vec2 offset = (originSize - newSize) / 2.0;  // (4)
            vec2 position = offset + mod(textureCoordinate * min(horizontalCount, verticalCount), newSize);  // (5)

            gl_FragColor = texture2D(inputTexture, position);  // (6)
        }
);

GLCommonFrameFilter::GLCommonFrameFilter(float horizontal, float vertical) {
    this->horizontal = horizontal;
    this->vertical = vertical;
}

void GLCommonFrameFilter::initProgram() {
    initProgram(kDefaultVertexShader.c_str(), kCommonFrameFragmentShader.c_str());
}

void GLCommonFrameFilter::initProgram(const char *vertexShader, const char *fragmentShader) {
    GLFilter::initProgram(vertexShader, fragmentShader);
    if (isInitialized()) {
        horizontalHandle = glGetUniformLocation(programHandle, "horizontal");
        verticalHandle = glGetUniformLocation(programHandle, "vertical");
    }
}

void GLCommonFrameFilter::onDrawBegin() {
    GLFilter::onDrawBegin();
    if (isInitialized()) {
        glUniform1f(horizontalHandle, horizontal);
        glUniform1f(verticalHandle, vertical);
    }
}

