# 添加目录
set(common_dir ../Common)

# 添加头文件路径
include_directories(${common_dir} )

# 添加源文件
# 添加源文件
add_library( media_recorder

        SHARED

        # 通用类

        # 录制器
        egl_share_context.cpp
        egl_core.cpp
        MediaRecorder.cpp
        com_cgfay_media_recorder_MediaRecorder.cpp)

# 链接静态库
target_link_libraries( media_recorder

        ffmpeg
        yuv

        -landroid
        -llog
        -lOpenSLES
        -lEGL
        ${GLES-lib} )
