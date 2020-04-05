# 设置cmake最低版本
cmake_minimum_required(VERSION 3.4.1)

# 设置GCC编译器的编译选项
if(CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-declarations -Wmissing-prototypes")
endif(CMAKE_COMPILER_IS_GNUCC)

# 设置clang编译器的编译选项
if(CMAKE_COMPILER_IS_CLANG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-declarations -Wmissing-prototypes -Wdocumentation -Wunreachable-code")
endif(CMAKE_COMPILER_IS_CLANG)

# 添加include目录路径
include_directories( include )

# 添加ffmpeg库
add_library( ffmpeg SHARED IMPORTED )
set_target_properties( ffmpeg
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libffmpeg.so )
                       
# 添加x264库
add_library( x264 STATIC IMPORTED )
set_target_properties( x264
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libx264.a )

# 添加faac库
add_library( faac STATIC IMPORTED )
set_target_properties( faac
                       PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libfaac.a )

# 添加yuv库
add_library(yuv SHARED IMPORTED )
set_target_properties( yuv
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libyuv.so )

# FFmpeg命令行库
add_subdirectory(${CMAKE_SOURCE_DIR}/FFCommand)

# 媒体录制器
add_subdirectory(${CMAKE_SOURCE_DIR}/FFMediaRecorder)

# 媒体录制器
#add_subdirectory(${CMAKE_SOURCE_DIR}/AVRecorder)

# MediaRecorder
add_subdirectory(${CMAKE_SOURCE_DIR}/MediaRecorder)

# MiniRecorder
add_subdirectory(${CMAKE_SOURCE_DIR}/MiniRecorder)

add_subdirectory(${CMAKE_SOURCE_DIR}/VideoEditor2)

# FLV录制器
add_subdirectory(${CMAKE_SOURCE_DIR}/FLVRecorder)

# Metadata库
add_subdirectory(${CMAKE_SOURCE_DIR}/Metadata)

# SoundTouch库
add_subdirectory(${CMAKE_SOURCE_DIR}/SoundTouch)

# GLFilter库
add_subdirectory(${CMAKE_SOURCE_DIR}/CainFilter)

# 媒体播放器
add_subdirectory(${CMAKE_SOURCE_DIR}/MediaPlayer)

# 媒体编辑器
add_subdirectory(${CMAKE_SOURCE_DIR}/MediaEditor)

# Video编辑器，用最简洁到代码实现功能，麻雀虽小五脏俱全
#add_subdirectory(${CMAKE_SOURCE_DIR}/VideoEditor)

# AudioEffect
#add_subdirectory(${CMAKE_SOURCE_DIR}/AudioEffect)

# VideoEffect
add_subdirectory(${CMAKE_SOURCE_DIR}/VideoEffect)

# ttmplayer
add_subdirectory(${CMAKE_SOURCE_DIR}/ttmplayer)

# ttvideoeditor
add_subdirectory(${CMAKE_SOURCE_DIR}/ttvideoeditor)

# ttvideorecorder
add_subdirectory(${CMAKE_SOURCE_DIR}/ttvideorecorder)



