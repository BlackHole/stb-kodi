set(SOURCES GraphicContext.cpp
            OSScreenSaver.cpp
            Resolution.cpp
            WinSystem.cpp)

set(HEADERS GraphicContext.h
            OSScreenSaver.h
            Resolution.h
            WinEvents.h
            WinSystem.h
            XBMC_events.h
            VideoSync.h)

if(CORE_PLATFORM_NAME_LC STREQUAL vuplus)
  list(APPEND SOURCES WinEventsLinux.cpp)
  list(APPEND HEADERS WinEventsLinux.h)
endif()

if(CORE_PLATFORM_NAME_LC STREQUAL rbpi)
  add_subdirectory(rpi)
endif()

if(CORE_PLATFORM_NAME_LC STREQUAL vuplus)
  add_subdirectory(vuplus)
endif()

core_add_library(windowing)
