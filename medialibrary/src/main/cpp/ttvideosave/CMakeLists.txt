# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

set(lib_src_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__STDC_CONSTANT_MACROS")
set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -D__STDC_CONSTANT_MACROS")

include_directories(
            ${CMAKE_SOURCE_DIR}/ttcommon
            ${CMAKE_SOURCE_DIR}/ttvideosave/video_encoder
            ${CMAKE_SOURCE_DIR}/ttvideosave/audio_effect
            ${CMAKE_SOURCE_DIR}/ttvideosave/video_consumer
            ${CMAKE_SOURCE_DIR}/ttvideosave/liblivecore)


add_subdirectory(${lib_src_DIR}/audio_effect)
add_subdirectory(${lib_src_DIR}/liblivecore)
add_subdirectory(${lib_src_DIR}/music_decoder)
add_subdirectory(${lib_src_DIR}/recorder_processor)
add_subdirectory(${lib_src_DIR}/video_consumer)
add_subdirectory(${lib_src_DIR}/video_encoder)


add_library(ttvideosave SHARED
            video_consumer/jni/VideoStudio.cpp
            recorder_processor/jni/NativeRecordProcessor.cpp
            music_decoder/jni/MusicDecoder.cpp
            )

target_link_libraries(ttvideosave
				libcommontool
                libaudioeffect
                libmusicdecoder
                librecordprocessor
                libvideoencoder
                liblivecore
                libvideoconsumer
				
				ffmpeg
				GLESv2
                OpenSLES
                EGL
                z
                gomp
                android
                log
                )