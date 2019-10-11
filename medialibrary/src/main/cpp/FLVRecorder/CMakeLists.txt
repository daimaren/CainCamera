# 添加目录
set(common_dir ../Common)

# 添加头文件路径
include_directories(${common_dir} )
include_directories(common)

# 添加源文件
add_library( flv_recorder

             SHARED

             # 通用类
             ${common_dir}/AVFrameFilter.cpp
             ${common_dir}/AVMediaWriter.cpp
             ${common_dir}/AVFormatter.cpp
             ${common_dir}/AVMediaData.cpp
             ${common_dir}/YuvConvertor.cpp
             ${common_dir}/YuvData.cpp
             common/aw_alloc.c
             common/aw_array.c
             common/aw_data.c
             common/aw_dict.c
             common/aw_file.c
             common/aw_thread_poll.c
             # encoder & muxer
             aw_x264.c
             aw_sw_x264_encoder.c
             aw_faac.c
             aw_sw_faac_encoder.c
             aw_encode_flv.c
             # 录制器
             FFMediaRecorder.cpp
             RecordParams.cpp
             com_cgfay_media_recorder_FFMediaRecorder.cpp)

# 链接静态库
target_link_libraries( flv_recorder

                       ffmpeg
                       x264
                       faac
                       yuv

                       android
                       jnigraphics
                       log )

