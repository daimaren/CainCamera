set(PATH_TO_PRE_BUILT ${CMAKE_SOURCE_DIR}/thirdparty/prebuilt/armeabi-v7a)

add_definitions("
-DGL_GLEXT_PROTOTYPES
-DEGL_EGLEXT_PROTOTYPES
")

# 添加头文件路径
include_directories(../thirdparty/ffmpeg/include/)
include_directories(libpng/)
include_directories(libcommon/)

# 添加源文件
file(GLOB PNG_LIB_SOURCE "3rdparty/libpng/*.c")
file(GLOB MARKUP_LIB_SOURCE "3rdparty/markup/*.cpp")
file(GLOB JSONCPP_LIB_SOURCE "3rdparty/jsoncpp/*.cpp")
file(GLOB COMMON_MODULE_SOURCE "common/*.cpp")
file(GLOB DECODER_MODULE_SOURCE "decoder/*.c*")
file(GLOB EDIT_MODEL_BASE_MODULE_SOURCE "editmodel/*.cpp")
file(GLOB EFFECT_BASE_MODULE_SOURCE "videoeffect/*.cpp")

file(GLOB FILES_LIB_COMMON_EGL_CORE "common/egl_core/*.cpp")

add_library( video_effect

        SHARED

        # 公共类
		libcommon/thread.cpp
		
		libcommon/message_queue/handler.cpp
		libcommon/message_queue/message_queue.cpp 

		libcommon/egl_core/egl_core.cpp
		libcommon/egl_core/egl_share_context.cpp

		libcommon/opensl_media/opensl_es_context.cpp

		libcommon/opengl_media/movie_frame.cpp
		libcommon/opengl_media/texture/texture_frame.cpp
		libcommon/opengl_media/texture/gpu_texture_frame.cpp
		libcommon/opengl_media/texture/yuv_texture_frame.cpp
		libcommon/opengl_media/texture_copier/texture_frame_copier.cpp
		libcommon/opengl_media/texture_copier/gpu_texture_frame_copier.cpp
		libcommon/opengl_media/texture_copier/yuv_texture_frame_copier.cpp
		libcommon/opengl_media/render/video_gl_surface_render.cpp

		# 核心代码
		gpu_texture_cache.cpp
		video_effect_processor.cpp
		
        ${PNG_LIB_SOURCE}
        ${MARKUP_LIB_SOURCE}
        ${JSONCPP_LIB_SOURCE}
		${COMMON_MODULE_SOURCE}
		${DECODER_MODULE_SOURCE}
		${EDIT_MODEL_BASE_MODULE_SOURCE}
		${EFFECT_BASE_MODULE_SOURCE}
		
        # 滤镜特效
        editmodel/overlay_scene/overlay_scene_filter.cpp
		videoeffect/overlay_scene/overlay_scene_effect.cpp
		
		editmodel/header_screen_scene/header_screen_scene_filter.cpp
		videoeffect/header_screen_scene/header_screen_scene_effect.cpp
		
		editmodel/header_fade_scene/header_fade_scene_filter.cpp
		videoeffect/header_fade_scene/header_fade_scene_effect.cpp
		
		editmodel/mask_alpha/model_mask_alpha_filter.cpp
		videoeffect/mask_alpha/mask_alpha_effect.cpp
		
		editmodel/text_scene/text_scene_filter.cpp
		videoeffect/text_scene/text_scene_effect.cpp
		
		editmodel/video_decoder_scene/model_decoder_filter.cpp
		videoeffect/gif_scene/gif_scene_effect.cpp
		
		editmodel/png_sequence/png_sequence_filter.cpp
		videoeffect/png_sequence/png_sequence_effect.cpp
		
		videoeffect/screen_scene/screen_scene_effect.cpp
		
		editmodel/video_overlay_scene/video_overlay_scene_filter.cpp
		videoeffect/video_overlay_scene/video_overlay_scene_effect.cpp
		
		editmodel/transition_scene/transition_scene_filter.cpp
		videoeffect/transition_scene/transition_scene_effect.cpp
		
		videoeffect/image_effect/image_base_effect.cpp
		videoeffect/image_effect/thin/thin_whitening_effect.cpp
		videoeffect/image_effect/thin/thin_effect.cpp
		videoeffect/image_effect/hue_effect.cpp
		videoeffect/image_effect/beautify_face/beautify_face_cool_effect.cpp
		videoeffect/image_effect/beautify_face/beautify_face_effect.cpp
		videoeffect/image_effect/beautify_face/smooth_skin_effect.cpp
		videoeffect/image_effect/beautify_face/thin_beautify_face_effect.cpp
		videoeffect/image_effect/whitening/whitening_cool_effect.cpp
		videoeffect/image_effect/whitening/whitening_effect.cpp
		videoeffect/image_effect/whitening/whitening_hor_effect.cpp
		videoeffect/image_effect/whitening/whitening_ver_effect.cpp
		videoeffect/image_effect/grayscale_effect.cpp
		videoeffect/image_effect/mix_effect.cpp
		videoeffect/image_effect/sepia_effect.cpp
		videoeffect/image_effect/tone_curve_effect.cpp
		videoeffect/image_effect/vignette_effect.cpp
		videoeffect/image_effect/sharpen_effect.cpp
		videoeffect/image_effect/image_effect_group.cpp
		videoeffect/image_effect/highpass/high_pass_effect.cpp
		videoeffect/image_effect/player_contrast_effect.cpp
		videoeffect/image_effect/contrast_effect.cpp
		
		editmodel/trailer_scene/trailer_scene_filter.cpp
		videoeffect/trailer_scene/trailer_scene_effect.cpp

		editmodel/blur_scene/blur_scene_filter.cpp
		editmodel/selective_blur_scene/selective_blur_scene_filter.cpp
		videoeffect/blur_scene/selective_blur_scene_effect.cpp
		videoeffect/blur_scene/blur_scene_effect.cpp

		videoeffect/saturation_scene/saturation_effect.cpp

		videoeffect/image_effect/assistant_util/DelayedFrameTexture.cpp
		videoeffect/image_effect/assistant_util/image_effect_graph.cpp
		videoeffect/image_effect/assistant_util/image_mix_effect.cpp
		videoeffect/image_effect/assistant_util/image_transform_effect.cpp
		videoeffect/image_effect/ghost_scene/blur_mirror/image_blur_mirror_effect.cpp
		videoeffect/image_effect/ghost_scene/carousel/image_carousel_effect.cpp
		videoeffect/image_effect/ghost_scene/soul_stuff/image_soul_stuff_effect.cpp
		videoeffect/image_effect/ghost_scene/tilt_shift/image_tilt_shift_effect.cpp
		videoeffect/image_effect/ghost_scene/x_ray/image_color_invert_effect.cpp
		videoeffect/image_effect/ghost_scene/image_2mirror_effect.cpp
		videoeffect/image_effect/ghost_scene/image_4mirror_effect.cpp
		videoeffect/image_effect/time_stretch/time_stretch_effect.cpp
        )
target_link_libraries(video_effect
        # 引入系统的动态库
        log
        android
        GLESv2
        EGL
        z
		OpenSLES
        # 引入ffmpeg库
        ${PATH_TO_PRE_BUILT}/libavfilter.a
        ${PATH_TO_PRE_BUILT}/libavformat.a
        ${PATH_TO_PRE_BUILT}/libavcodec.a
        ${PATH_TO_PRE_BUILT}/libpostproc.a
        ${PATH_TO_PRE_BUILT}/libswresample.a
        ${PATH_TO_PRE_BUILT}/libswscale.a
        ${PATH_TO_PRE_BUILT}/libavutil.a
        ${PATH_TO_PRE_BUILT}/libpostproc.a
        ${PATH_TO_PRE_BUILT}/libfdk-aac.a
        ${PATH_TO_PRE_BUILT}/libvo-aacenc.a
        ${PATH_TO_PRE_BUILT}/libx264.a
        ${PATH_TO_PRE_BUILT}/libsox.a
        )

