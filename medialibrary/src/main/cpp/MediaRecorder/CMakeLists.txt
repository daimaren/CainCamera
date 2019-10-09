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
        live_audio_packet_queue.cpp
        live_video_packet_queue.cpp
        live_common_packet_pool.cpp
        live_yuy2_packet_pool.cpp
        host_gpu_copier.cpp
        handler.cpp
        message_queue.cpp
        matrix.cpp
        texture_frame_copier.cpp
        gpu_texture_frame_copier.cpp
        texture_frame.cpp
        gpu_texture_frame.cpp
        video_gl_surface_render.cpp
        recording_preview_renderer.cpp
        recording_preview_controller.cpp
        video_x264_encoder.cpp
        video_encoder_adapter.cpp
        soft_encoder_adapter.cpp
        hw_encoder_adapter.cpp
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
