# 添加目录
set(common_dir ../Common)

# 添加头文件路径
include_directories(${common_dir} )
include_directories(common)

# 添加源文件
add_library( mini_recorder

             SHARED
             # 通用类
             # encoder & muxer
             #MediaWriter.cpp
             Handler.cpp
             MsgQueue.cpp
             live_common_packet_pool.cpp
             live_packet_pool.cpp
             live_audio_packet_queue.cpp
             live_video_packet_queue.cpp
             # 录制器
             MiniRecorder.cpp
             RecordParams.cpp
             com_cgfay_media_recorder_MiniRecorder.cpp)

# 链接静态库
target_link_libraries( mini_recorder
                       ffmpeg
                       yuv

                       android
                       jnigraphics
                       log
                       -lEGL
                       GLESv2)

