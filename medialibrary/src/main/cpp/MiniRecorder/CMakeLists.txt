# 添加目录
set(common_dir ../Common)

# 添加头文件路径
include_directories(${common_dir} )
include_directories(common)

# 添加源文件
add_library( mini_recorder

             SHARED

             # 通用类
             ${common_dir}/AVFormat.cpp
             ${common_dir}/AVMediaData.cpp
             ${common_dir}/YuvConvertor.cpp
             ${common_dir}/YuvData.cpp
#             ../CainFilter/common/CainEGLContext.cpp
#             ../CainFilter/common/EglHelper.cpp
#             ../CainFilter/common/CoordinateUtils.cpp
#             ../CainFilter/common/OpenGLUtils.cpp
             common/aw_alloc.c
             common/aw_array.c
             common/aw_data.c
             common/aw_dict.c
             common/aw_file.c
             common/aw_thread_poll.c
             # encoder & muxer
             MediaWriter.cpp
             Handler.cpp
             MsgQueue.cpp
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

