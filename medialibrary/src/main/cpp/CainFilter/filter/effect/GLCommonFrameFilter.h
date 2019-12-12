//
// Created by 1 on 2019/12/12.
//

#ifndef CAINCAMERA_GLCOMMONFRAMEFILTER_H
#define CAINCAMERA_GLCOMMONFRAMEFILTER_H


#include "../GLFilter.h"

class GLCommonFrameFilter : public GLFilter{
public:
    void initProgram(const char *vertexShader, const char *fragmentShader) override;
};


#endif //CAINCAMERA_GLCOMMONFRAMEFILTER_H
