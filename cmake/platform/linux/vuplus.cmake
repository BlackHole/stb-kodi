set(PLATFORM_REQUIRED_DEPS OpenGLES EGL Vuplus Gstreamer Xkbcommon)
set(APP_RENDER_SYSTEM gles)
list(APPEND PLATFORM_DEFINES -D_ARMEL -DTARGET_VUPLUS -DTARGET_STB -DTARGET_STB_EXTEND)
