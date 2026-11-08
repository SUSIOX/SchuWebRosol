# SchuWebRosol - web page as a live Spout (Windows) / Syphon (macOS) source.
cmake_minimum_required(VERSION 3.21)

set(CMAKE_CONFIGURATION_TYPES Debug Release)

project(SchuWebRosol VERSION 0.1.0 LANGUAGES C CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# CEF binary distribution (https://cef-builds.spotifycdn.com/index.html).
set(CEF_VERSION "156.0.26+g8ea0c73+chromium-156.0.8078.88" CACHE STRING "CEF version")

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
  if("${PROJECT_ARCH}" STREQUAL "arm64")
    set(CEF_PLATFORM "macosarm64")
  elseif("${PROJECT_ARCH}" STREQUAL "x86_64")
    set(CEF_PLATFORM "macosx64")
  elseif("${CMAKE_HOST_SYSTEM_PROCESSOR}" STREQUAL "arm64")
    set(PROJECT_ARCH "arm64")
    set(CEF_PLATFORM "macosarm64")
  else()
    set(PROJECT_ARCH "x86_64")
    set(CEF_PLATFORM "macosx64")
  endif()
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
    set(CEF_PLATFORM "linuxarm64")
  else()
    set(CEF_PLATFORM "linux64")
  endif()
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  if("${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" STREQUAL "ARM64")
    set(CEF_PLATFORM "windowsarm64")
  else()
    set(CEF_PLATFORM "windows64")
  endif()
endif()

set(CEF_DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/third_party/cef" CACHE PATH "Where the CEF distribution is downloaded")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(DownloadCEF)
DownloadCEF("${CEF_PLATFORM}" "minimal" "${CEF_VERSION}" "${CEF_DOWNLOAD_DIR}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")

# The sandbox requires the bootstrap executable on Windows; it is not needed
# for a local VJ tool.
set(USE_SANDBOX OFF CACHE BOOL "Enable or disable use of the sandbox." FORCE)

find_package(CEF REQUIRED)

add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)
add_subdirectory(src)

PRINT_CEF_CONFIG()
