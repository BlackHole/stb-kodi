#.rst:
# FindGStreamer
# -------
# Finds the GStreamer library
#
# This will define the following variables::
#
# GSTREAMER_FOUND - system has GStreamer
# GSTREAMER_INCLUDE_DIRS - the GStreamer include directory
# GSTREAMER_LIBRARIES - the GStreamer libraries
# GSTREAMER_DEFINITIONS - the GStreamer compile definitions

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_GLIB glib-2.0 QUIET)
  pkg_check_modules(PC_GOBJECT gobject-2.0 QUIET)
  pkg_check_modules(PC_GSTREAMER gstreamer-1.0 QUIET)
  pkg_check_modules(PC_GSTREAMERPBUTILS gstreamer-pbutils-1.0 QUIET)
endif()

find_path(GSTREAMER_INCLUDE_DIR NAMES gst/gst.h
                          PATHS ${PC_GSTREAMER_INCLUDE_DIRS})
find_library(GLIB_LIBRARY NAMES glib-2.0
                         PATHS ${PC_GLIB_LIBDIR})
find_library(GOBJECT_LIBRARY NAMES gobject-2.0
                         PATHS ${PC_GOBJECT_LIBDIR})
find_library(GSTREAMER_LIBRARY NAMES gstreamer-1.0
                         PATHS ${PC_GSTREAMER_LIBDIR})
find_library(GSTREAMERPBUTILS_LIBRARY NAMES gstreamer-pbutils-1.0
                         PATHS ${PC_GSTREAMERPBUTILS_LIBDIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Gstreamer REQUIRED_VARS GSTREAMER_LIBRARY GSTREAMER_INCLUDE_DIR)

if(GSTREAMER_FOUND)
  set(GSTREAMER_LIBRARIES ${GSTREAMER_LIBRARY} ${GSTREAMERPBUTILS_LIBRARY} ${GOBJECT_LIBRARY} ${GLIB_LIBRARY})
  set(GSTREAMER_INCLUDE_DIRS ${GSTREAMER_INCLUDE_DIR} ${GSTREAMERPBUTILS_INCLUDE_DIR})
  set(GSTREAMER_DEFINITIONS -DHAVE_LIBGSTREAMER=1)
endif()

mark_as_advanced(GSTREAMER_LIBRARY GSTREAMER_INCLUDE_DIR GSTREAMER_LDFLAGS)
