set(SOURCES GSTPlayer.cpp)

set(HEADERS GSTPlayer.h)

core_add_library(gstplayer)
target_compile_definitions(${CORE_LIBRARY} PRIVATE -D__STDC_FORMAT_MACROS)
