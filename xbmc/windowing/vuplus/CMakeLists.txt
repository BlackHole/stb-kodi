set(SOURCES WinSystemVuplus.cpp
            VuplusUtils.cpp)

set(HEADERS WinSystemVuplus.h
            VuplusUtils.h)

if(OPENGLES_FOUND)
  list(APPEND SOURCES WinSystemVuplusGLESContext.cpp)
  list(APPEND HEADERS WinSystemVuplusGLESContext.h)
endif()

core_add_library(windowing_Vuplus)
