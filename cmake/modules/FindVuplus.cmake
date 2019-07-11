#.rst:
# FindVuplus
# -------
# Finds the Vuplus library
#
# This will define the following variables::
#
# VUPLUS_FOUND - system has VUPLUS
# VUPLUS_INCLUDE_DIRS - the VUPLUS include directory
# VUPLUS_LIBRARIES - the VUPLUS libraries
# VUPLUS_DEFINITIONS - the VUPLUS definitions
#
# and the following imported targets::
#
#   VUPLUS::VUPLUS   - The VUPLUS library

find_path(VUPLUS_INCLUDE_DIR KodiGLESPL.h)

find_library(VUPLUS_LIBRARY NAMES KodiGLESPL)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VUPLUS
                                  REQUIRED_VARS VUPLUS_LIBRARY VUPLUS_INCLUDE_DIR)

if(VUPLUS_FOUND)
  set(VUPLUS_LIBRARIES ${VUPLUS_LIBRARY})
  set(VUPLUS_INCLUDE_DIRS ${VUPLUS_INCLUDE_DIR})
  set(VUPLUS_DEFINITIONS -DHAS_VUPLUS=1)

  if(NOT TARGET VUPLUS::VUPLUS)
    add_library(VUPLUS::VUPLUS UNKNOWN IMPORTED)
    set_target_properties(VUPLUS::VUPLUS PROPERTIES
                                   IMPORTED_LOCATION "${VUPLUS_LIBRARY}"
                                   INTERFACE_INCLUDE_DIRECTORIES "${VUPLUS_INCLUDE_DIR}"
                                   INTERFACE_COMPILE_DEFINITIONS HAS_VUPLUS=1)
  endif()
endif()

mark_as_advanced(VUPLUS_INCLUDE_DIR VUPLUS_LIBRARY)
