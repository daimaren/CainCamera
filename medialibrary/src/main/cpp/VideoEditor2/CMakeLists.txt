# 添加目录

# 添加头文件路径

# 添加源文件
add_library( video_editor2

             SHARED

             # 通用类
             movie_frame.cpp
             # 编辑器
             VideoEditor.cpp
             com_cgfay_media_editor_VideoEditor.cpp)

# 链接静态库
target_link_libraries( video_editor2

                       android
                       jnigraphics
                       log
                       -lEGL
                       GLESv2)

