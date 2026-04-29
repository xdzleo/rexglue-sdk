
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was rexglueConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# Full version string including any -dev/-rc identifier.
# The plain rexglue_VERSION (set by CMake from rexglueConfigVersion.cmake) holds
# the numeric triple/quad only.
set(REXGLUE_VERSION_STRING "0.7.7.21-dev.gb468316")

# SDK share directory (entry point sources, etc.)
set(REXGLUE_SHARE_DIR "${PACKAGE_PREFIX_DIR}/share/rexglue")

include(CMakeFindDependencyMacro)

# Helper functions for consumer projects
include("${CMAKE_CURRENT_LIST_DIR}/rexglue_helpers.cmake")

# Ensure bundled dependencies are found from the SDK install prefix
list(PREPEND CMAKE_PREFIX_PATH "${PACKAGE_PREFIX_DIR}")

# Required dependencies
find_dependency(fmt CONFIG)
find_dependency(spdlog CONFIG)
find_dependency(Snappy CONFIG)
find_dependency(utf8cpp CONFIG)
find_dependency(SDL3 CONFIG)

# Vulkan dependencies
set(REXGLUE_USE_VULKAN OFF)
if(REXGLUE_USE_VULKAN)
    find_dependency(VulkanHeaders CONFIG)
    find_dependency(volk CONFIG)
    find_dependency(VulkanMemoryAllocator CONFIG)
endif()

# Include the generated targets
include("${CMAKE_CURRENT_LIST_DIR}/rexglueTargets.cmake")

if(NOT rexglue_FIND_QUIETLY)
    message(STATUS "Found ReXGlue SDK ${REXGLUE_VERSION_STRING} at (${PACKAGE_PREFIX_DIR})")
endif()

check_required_components(rexglue)
