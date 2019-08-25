set(SOURCES "")
set(HEADERS "")

if(DBUS_FOUND)
  list(APPEND SOURCES OSScreenSaverFreedesktop.cpp)
  list(APPEND HEADERS OSScreenSaverFreedesktop.h)
endif()

if(CORE_PLATFORM_NAME_LC STREQUAL vuplus)
  list(APPEND SOURCES WinEventsLinux.cpp)
  list(APPEND HEADERS WinEventsLinux.h)
endif()


if(SOURCES)
  core_add_library(windowing_linux)
endif()