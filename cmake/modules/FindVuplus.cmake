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


find_path(VUPLUS_ARM_INCLUDE_DIR KodiGLESPL.h)
find_library(VUPLUS_ARM_LIBRARY NAMES KodiGLESPL)

find_path(VUPLUS_MIPSEL_INCLUDE_DIR gles_init.h)
find_library(VUPLUS_ARM_LIBRARY NAMES xbmc_base)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VUPLUS_ARM
                                  REQUIRED_VARS VUPLUS_ARM_LIBRARY VUPLUS_ARM_INCLUDE_DIR)

find_package_handle_standard_args(VUPLUS_MIPSEL
                                  REQUIRED_VARS VUPLUS_MIPSEL_LIBRARY VUPLUS_MIPSEL_INCLUDE_DIR)

if(VUPLUS_ARM_FOUND)
  set(VUPLUS_LIBRARIES ${VUPLUS_ARM_LIBRARY})
  set(VUPLUS_INCLUDE_DIRS ${VUPLUS_ARM_INCLUDE_DIR})
  set(VUPLUS_DEFINITIONS -DHAS_VUPLUS_ARM=1)

  if(NOT TARGET VUPLUS::VUPLUS)
    add_library(VUPLUS::VUPLUS UNKNOWN IMPORTED)
    set_target_properties(VUPLUS::VUPLUS PROPERTIES
                                   IMPORTED_LOCATION "${VUPLUS_ARM_LIBRARY}"
                                   INTERFACE_INCLUDE_DIRECTORIES "${VUPLUS_ARM_INCLUDE_DIR}"
                                   INTERFACE_COMPILE_DEFINITIONS HAS_VUPLUS_ARM=1)
  endif()
endif()


if(VUPLUS_MIPSEL_FOUND)
  set(VUPLUS_LIBRARIES ${VUPLUS_MIPSEL_LIBRARY})
  set(VUPLUS_INCLUDE_DIRS ${VUPLUS_MIPSEL_INCLUDE_DIR})
  set(VUPLUS_DEFINITIONS -DHAS_VUPLUS_MIPSEL=1)

  if(NOT TARGET VUPLUS::VUPLUS)
    add_library(VUPLUS::VUPLUS UNKNOWN IMPORTED)
    set_target_properties(VUPLUS::VUPLUS PROPERTIES
                                   IMPORTED_LOCATION "${VUPLUS_MIPSEL_LIBRARY}"
                                   INTERFACE_INCLUDE_DIRECTORIES "${VUPLUS_MIPSEL_INCLUDE_DIR}"
                                   INTERFACE_COMPILE_DEFINITIONS HAS_VUPLUS_MIPSEL=1)
  endif()
endif()

mark_as_advanced(VUPLUS_ARM_INCLUDE_DIR VUPLUS_ARM_LIBRARY VUPLUS_MIPSEL_INCLUDE_DIR VUPLUS_MIPSEL_LIBRARY)
