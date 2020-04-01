#include "opensl_es_context.h"

#define LOG_TAG "OpenSLESContext"

OpenSLESContext* OpenSLESContext::instance = new OpenSLESContext();

void OpenSLESContext::init() {
	ALOGI("createEngine");
	//创建 opensl 引擎
	SLresult result = createEngine();
	ALOGI("createEngine result is s%", ResultToString(result));
	if (SL_RESULT_SUCCESS == result) {
		ALOGI("Realize the engine object");
		// Realize the engine object - 实例化引擎
		result = RealizeObject(engineObject);
		if (SL_RESULT_SUCCESS == result) {
			ALOGI("Get the engine interface");
			// Get the engine interface
			result = GetEngineInterface();
		}
	}
}

OpenSLESContext::OpenSLESContext() {
	isInited = false;
}
OpenSLESContext::~OpenSLESContext() {
}

OpenSLESContext* OpenSLESContext::GetInstance() {
	if (!instance->isInited) {
		instance->init();
		instance->isInited = true;
	}
	return instance;
}
