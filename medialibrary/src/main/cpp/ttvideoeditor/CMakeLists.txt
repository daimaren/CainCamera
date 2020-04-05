
# 根据API版本判断使用哪个版本的OpenGLES
if (${ANDROID_PLATFORM_LEVEL} LESS 12)
  message(FATAL_ERROR "OpenGL 2 is not supported before API level 11 (currently using ${ANDROID_PLATFORM_LEVEL}).")
  return()
elseif (${ANDROID_PLATFORM_LEVEL} LESS 18)
  add_definitions("-DDYNAMIC_ES3")
  set(GLES-lib GLESv2)
else ()
  set(GLES-lib GLESv3)
endif (${ANDROID_PLATFORM_LEVEL} LESS 11)

# 添加audioeffect动态库

# 添加videoeffect库

# 添加头文件路径

# 添加源文件
add_library( ttvideoeditor

             SHARED

             # library

             # controller
             )

# 链接静态库
target_link_libraries( ttvideoeditor

                       ttmplayer

                       -landroid
                       -llog
                       -lOpenSLES
                       -lEGL
                       ${GLES-lib} )

