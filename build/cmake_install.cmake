# Install script for directory: C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/ReXGlue")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/Program Files/LLVM/bin/llvm-objdump.exe")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/thirdparty/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/src/core/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/src/filesystem/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/src/ui/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/src/input/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/src/audio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/src/graphics/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/src/system/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/src/kernel/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/src/codegen/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/src/rexglue/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/rexcore.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/rexfilesystem.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/rexui.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/rexinput.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/rexaudio.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/rexgraphics.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/rexsystem.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/rexkernel.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/rexcodegen.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/aes128.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/mspack.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/o1heap.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/disasm.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/xxhash.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/imgui.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/libavcodec.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/libavutil.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/rexglue.exe")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/rexglue.exe" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/rexglue.exe")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "C:/Program Files/LLVM/bin/llvm-strip.exe" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/rexglue.exe")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/out/win-amd64/TracyClient.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE DIRECTORY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/include/rex")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/rex" TYPE FILE FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/include/rex/version.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/disruptorplus" TYPE DIRECTORY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/disruptorplus/include/")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/renderdoc" TYPE DIRECTORY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/renderdoc/" FILES_MATCHING REGEX "/[^/]*\\.h$")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE DIRECTORY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/tomlplusplus/include/")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE DIRECTORY FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/simde/simde" FILES_MATCHING REGEX "/[^/]*\\.h$")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/xxHash/xxhash.h"
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/xxHash/xxh3.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/o1heap/o1heap/o1heap.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/imgui/imgui.h"
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/imgui/imconfig.h"
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/imgui/imgui_internal.h"
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/imgui/imstb_rectpack.h"
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/imgui/imstb_textedit.h"
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/imgui/imstb_truetype.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/rexglue" TYPE FILE FILES
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/src/ui/windowed_app_main_win.cpp"
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/src/ui/windowed_app_main_posix.cpp"
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/src/ui/rex_app.cpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/dxc" TYPE FILE FILES
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/dxc/include/DxbcConverter.h"
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/thirdparty/dxc/include/dxcapi.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/rexglue" TYPE FILE FILES
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/rexglueConfig.cmake"
    "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/rexglueConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/rexglue" TYPE FILE FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/cmake/rexglue_helpers.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/rexglue/rexglueTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/rexglue/rexglueTargets.cmake"
         "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/CMakeFiles/Export/3382210432fb09e8a8d2395d313f3a0f/rexglueTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/rexglue/rexglueTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/rexglue/rexglueTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/rexglue" TYPE FILE FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/CMakeFiles/Export/3382210432fb09e8a8d2395d313f3a0f/rexglueTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/rexglue" TYPE FILE FILES "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/CMakeFiles/Export/3382210432fb09e8a8d2395d313f3a0f/rexglueTargets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
      # Normalize path casing on Windows before hashing to avoid duplicate entries
    if(CMAKE_HOST_WIN32)
        string(TOLOWER "${CMAKE_INSTALL_PREFIX}" _reg_key)
    else()
        set(_reg_key "${CMAKE_INSTALL_PREFIX}")
    endif()
    string(MD5 _hash "${_reg_key}")

    if(CMAKE_HOST_WIN32)
        # Windows CMake User Package Registry lives in HKCU (not the filesystem)
        set(_reg_root "HKCU\\Software\\Kitware\\CMake\\Packages\\rexglue")
        execute_process(
            COMMAND reg add "${_reg_root}" /v "${_hash}" /t REG_SZ /d "${CMAKE_INSTALL_PREFIX}" /f
            OUTPUT_QUIET ERROR_QUIET
        )
    else()
        set(_reg_dir "$ENV{HOME}/.cmake/packages/rexglue")
        file(MAKE_DIRECTORY "${_reg_dir}")
        file(WRITE "${_reg_dir}/${_hash}" "${CMAKE_INSTALL_PREFIX}")
    endif()
    message(STATUS "Registered rexglue in CMake user package registry")
    message(STATUS "  -> ${CMAKE_INSTALL_PREFIX}")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk/build/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
