# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

set(PATH_TO_AV_ENGINE ${CMAKE_SOURCE_DIR}/ttmplayer)
set(PATH_TO_PRE_BUILT ${CMAKE_SOURCE_DIR}/ttmplayer/thirdparty/prebuilt/${ANDROID_ABI})

add_definitions("
-DGL_GLEXT_PROTOTYPES
-DEGL_EGLEXT_PROTOTYPES
")

include_directories(${PATH_TO_AV_ENGINE}/)
include_directories(${PATH_TO_AV_ENGINE}/common/)
include_directories(${PATH_TO_AV_ENGINE}/video_player/)
include_directories(../VideoEffect)
include_directories(../ttvideosave)

file(GLOB FILES_LIB_COMMON "${PATH_TO_AV_ENGINE}/common/*.cpp")
file(GLOB FILES_LIB_COMMON_EGL_CORE "${PATH_TO_AV_ENGINE}/common/egl_core/*.cpp")
file(GLOB FILES_LIB_COMMON_EGL_CORE "${PATH_TO_AV_ENGINE}/common/egl_core/*.cpp")
file(GLOB FILES_LIB_COMMON_RESAMPLER "${PATH_TO_AV_ENGINE}/common/libresampler/*.cpp")
file(GLOB FILES_LIB_COMMON_MSG_Q "${PATH_TO_AV_ENGINE}/common/message_queue/*.cpp")
file(GLOB FILES_LIB_COMMON_OPENGL_MEDIA "${PATH_TO_AV_ENGINE}/common/opengl_media/*.cpp")
file(GLOB FILES_LIB_COMMON_OPENGL_MEDIA_RENDER "${PATH_TO_AV_ENGINE}/common/opengl_media/render/*.cpp")
file(GLOB FILES_LIB_COMMON_OPENGL_MEDIA_TEXTURE "${PATH_TO_AV_ENGINE}/common/opengl_media/texture/*.cpp")
file(GLOB FILES_LIB_COMMON_OPENGL_MEDIA_TEX_COPIER "${PATH_TO_AV_ENGINE}/common/opengl_media/texture_copier/*.cpp")
file(GLOB FILES_LIB_COMMON_OPENSL "${PATH_TO_AV_ENGINE}/common/opensl_media/*.cpp")

file(GLOB FILES_LIB_VIDEO_PLAYER "${PATH_TO_AV_ENGINE}/video_player/*.cpp")
file(GLOB FILES_LIB_VIDEO_PLAYER_COMMON "${PATH_TO_AV_ENGINE}/video_player/common/*.cpp")
file(GLOB FILES_LIB_VIDEO_PLAYER_DECODER "${PATH_TO_AV_ENGINE}/video_player/decoder/*.cpp")
file(GLOB FILES_LIB_VIDEO_PLAYER_SYNC "${PATH_TO_AV_ENGINE}/video_player/sync/*.cpp")
file(GLOB FILES_LIB_VIDEO_PLAYER_TEX_UPLOADER "${PATH_TO_AV_ENGINE}/video_player/texture_uploader/*.cpp")

add_library(ttmplayer SHARED
        ${FILES_LIB_COMMON}
        ${FILES_LIB_COMMON_EGL_CORE}
        ${FILES_LIB_COMMON_MSG_Q}
        ${FILES_LIB_COMMON_RESAMPLER}
        ${FILES_LIB_COMMON_OPENSL}
        ${FILES_LIB_COMMON_OPENGL_MEDIA}
        ${FILES_LIB_COMMON_OPENGL_MEDIA_RENDER}
        ${FILES_LIB_COMMON_OPENGL_MEDIA_TEXTURE}
        ${FILES_LIB_COMMON_OPENGL_MEDIA_TEX_COPIER}
        ${FILES_LIB_VIDEO_PLAYER}
        ${FILES_LIB_VIDEO_PLAYER_COMMON}
        ${FILES_LIB_VIDEO_PLAYER_DECODER}
        ${FILES_LIB_VIDEO_PLAYER_SYNC}
        ${FILES_LIB_VIDEO_PLAYER_TEX_UPLOADER}
		android/AVMessageQueue.cpp
		android/CainMediaPlayer.cpp
		android/JNIHelp.cpp
		android/com_cgfay_media_CainMediaPlayer.cpp
        )

target_link_libraries(ttmplayer
                       # 引入系统的动态库
                       log
                       android
                       GLESv2
                       EGL
                       z
                       OpenSLES
					   # 引入第三方库
					   ffmpeg
                       video_effect
					   ttvideosave
                       # 引入静态库
                       ${PATH_TO_PRE_BUILT}/libfdk-aac.a
                       ${PATH_TO_PRE_BUILT}/libvo-aacenc.a
                       ${PATH_TO_PRE_BUILT}/libx264.a
					   ${PATH_TO_PRE_BUILT}/libsox.a
                       )