set(SWR_TARGET "SchuWebRosol")

set(SWR_SRCS
  app.cc app.h
  control_client.cc control_client.h
  control_window.cc control_window.h
  controller.cc controller.h
  frame_sender.h
  osr_client.cc osr_client.h
  paths.cc paths.h
  settings.cc settings.h
  )
set(SWR_SRCS_LINUX main_linux.cc frame_sender_linux.cc)
set(SWR_SRCS_WINDOWS main_win.cc frame_sender_win.cc)
set(SWR_SRCS_MAC main_mac.mm frame_sender_mac.mm)
APPEND_PLATFORM_SOURCES(SWR_SRCS)

set(SWR_HELPER_SRCS process_helper_mac.cc)

# Embed control.html into the executable.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/control.html" _html_hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _html_array "${_html_hex}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/control_html.h.tmp"
  "#include <cstddef>\nstatic const unsigned char kControlHtml[] = {${_html_array}};\nstatic const size_t kControlHtmlSize = sizeof(kControlHtml);\n")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/control_html.h.tmp"
               "${CMAKE_CURRENT_BINARY_DIR}/control_html.h" COPYONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS control.html)

if(NOT OS_MAC)
  ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
endif()

SET_CEF_TARGET_OUT_DIR()

macro(SWR_COMMON_TARGET target)
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_definitions(${target} PRIVATE SCHUWEBROSOL_VERSION="${PROJECT_VERSION}")
endmacro()

#
# Linux (development/testing only - no Spout/Syphon).
#
if(OS_LINUX)
  add_executable(${SWR_TARGET} ${SWR_SRCS})
  SET_EXECUTABLE_TARGET_PROPERTIES(${SWR_TARGET})
  SWR_COMMON_TARGET(${SWR_TARGET})
  add_dependencies(${SWR_TARGET} libcef_dll_wrapper)
  target_link_libraries(${SWR_TARGET} libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})
  set_target_properties(${SWR_TARGET} PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
    RUNTIME_OUTPUT_DIRECTORY ${CEF_TARGET_OUT_DIR})
  COPY_FILES("${SWR_TARGET}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
  COPY_FILES("${SWR_TARGET}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")
  if(EXISTS "${CEF_BINARY_DIR}/libminigbm.so")
    COPY_FILES("${SWR_TARGET}" "libminigbm.so" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
  endif()
endif()

#
# Windows: Spout2 (SpoutDX, sends CPU pixels through a shared DX11 texture).
#
if(OS_WINDOWS)
  include(FetchContent)
  set(SPOUT2_TAG "2.007.018" CACHE STRING "Spout2 release tag")
  FetchContent_Declare(spout2
    URL "https://github.com/leadedge/Spout2/archive/refs/tags/${SPOUT2_TAG}.tar.gz"
    SOURCE_SUBDIR "do-not-add-subdirectory")
  FetchContent_MakeAvailable(spout2)
  set(_spout_gl "${spout2_SOURCE_DIR}/SPOUTSDK/SpoutGL")
  set(_spout_dx "${spout2_SOURCE_DIR}/SPOUTSDK/SpoutDirectX/SpoutDX")
  add_library(spoutdx STATIC
    "${_spout_dx}/SpoutDX.cpp"
    "${_spout_gl}/SpoutCopy.cpp"
    "${_spout_gl}/SpoutDirectX.cpp"
    "${_spout_gl}/SpoutFrameCount.cpp"
    "${_spout_gl}/SpoutSenderNames.cpp"
    "${_spout_gl}/SpoutSharedMemory.cpp"
    "${_spout_gl}/SpoutUtils.cpp")
  target_include_directories(spoutdx PUBLIC "${_spout_dx}" "${_spout_gl}")
  target_compile_definitions(spoutdx PRIVATE _CRT_SECURE_NO_WARNINGS)
  set_target_properties(spoutdx PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_link_libraries(spoutdx PUBLIC d3d11 dxgi winmm shlwapi version comctl32)

  add_executable(${SWR_TARGET} WIN32 ${SWR_SRCS})
  SET_EXECUTABLE_TARGET_PROPERTIES(${SWR_TARGET})
  SWR_COMMON_TARGET(${SWR_TARGET})
  ADD_WINDOWS_MANIFEST("${CMAKE_CURRENT_SOURCE_DIR}/win" "${SWR_TARGET}" "exe")
  add_dependencies(${SWR_TARGET} libcef_dll_wrapper)
  target_link_libraries(${SWR_TARGET} libcef_lib libcef_dll_wrapper spoutdx ${CEF_STANDARD_LIBS})
  set_target_properties(${SWR_TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CEF_TARGET_OUT_DIR})
  COPY_FILES("${SWR_TARGET}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
  COPY_FILES("${SWR_TARGET}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")
endif()

#
# macOS: Syphon (Metal server), built from source with xcodebuild.
#
if(OS_MAC)
  include(ExternalProject)
  set(SYPHON_COMMIT "f4761677a45b8034a3c2069ec0f3d2553da81fba" CACHE STRING "Syphon-Framework commit")
  set(_syphon_out "${CMAKE_BINARY_DIR}/syphon")
  ExternalProject_Add(syphon_framework
    URL "https://github.com/Syphon/Syphon-Framework/archive/${SYPHON_COMMIT}.tar.gz"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND xcodebuild -project <SOURCE_DIR>/Syphon.xcodeproj -target Syphon
      -configuration Release ARCHS=${PROJECT_ARCH} ONLY_ACTIVE_ARCH=NO
      MACOSX_DEPLOYMENT_TARGET=12.0
      CODE_SIGNING_ALLOWED=NO CODE_SIGN_IDENTITY=
      SYMROOT=<BINARY_DIR>/build OBJROOT=<BINARY_DIR>/obj
      CONFIGURATION_BUILD_DIR=${_syphon_out}
      build
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS "${_syphon_out}/Syphon.framework/Syphon")

  set(SWR_APP "${CEF_TARGET_OUT_DIR}/${SWR_TARGET}.app")
  set(EXECUTABLE_NAME "${SWR_TARGET}")
  set(PRODUCT_NAME "${SWR_TARGET}")
  set(VERSION_SHORT "${PROJECT_VERSION}")

  add_executable(${SWR_TARGET} MACOSX_BUNDLE ${SWR_SRCS})
  SET_EXECUTABLE_TARGET_PROPERTIES(${SWR_TARGET})
  SWR_COMMON_TARGET(${SWR_TARGET})
  add_dependencies(${SWR_TARGET} libcef_dll_wrapper syphon_framework)
  target_compile_options(${SWR_TARGET} PRIVATE -fobjc-arc "-iframework${_syphon_out}")
  target_link_libraries(${SWR_TARGET} libcef_dll_wrapper ${CEF_STANDARD_LIBS}
    "-F${_syphon_out}" "-framework Syphon" "-framework Metal" "-framework Foundation")
  set_target_properties(${SWR_TARGET} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/mac/Info.plist.in"
    INSTALL_RPATH "@executable_path/../Frameworks"
    BUILD_WITH_INSTALL_RPATH TRUE
    RUNTIME_OUTPUT_DIRECTORY ${CEF_TARGET_OUT_DIR})

  COPY_MAC_FRAMEWORK("${SWR_TARGET}" "${CEF_BINARY_DIR}" "${SWR_APP}")
  add_custom_command(TARGET ${SWR_TARGET} POST_BUILD
    COMMAND rm -rf "${SWR_APP}/Contents/Frameworks/Syphon.framework"
    COMMAND cp -R "${_syphon_out}/Syphon.framework" "${SWR_APP}/Contents/Frameworks/"
    VERBATIM)

  set(SWR_HELPER_TARGET "${SWR_TARGET}_Helper")
  set(SWR_HELPER_OUTPUT_NAME "${SWR_TARGET} Helper")
  foreach(_suffix_list ${CEF_HELPER_APP_SUFFIXES})
    string(REPLACE ":" ";" _suffix_list ${_suffix_list})
    list(GET _suffix_list 0 _name_suffix)
    list(GET _suffix_list 1 _target_suffix)
    list(GET _suffix_list 2 _plist_suffix)

    set(_helper_target "${SWR_HELPER_TARGET}${_target_suffix}")
    set(_helper_output_name "${SWR_HELPER_OUTPUT_NAME}${_name_suffix}")

    set(_helper_info_plist "${CMAKE_CURRENT_BINARY_DIR}/helper-Info${_target_suffix}.plist")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mac/helper-Info.plist.in" _plist_contents)
    string(REPLACE "\${EXECUTABLE_NAME}" "${_helper_output_name}" _plist_contents ${_plist_contents})
    string(REPLACE "\${PRODUCT_NAME}" "${_helper_output_name}" _plist_contents ${_plist_contents})
    string(REPLACE "\${BUNDLE_ID_SUFFIX}" "${_plist_suffix}" _plist_contents ${_plist_contents})
    string(REPLACE "\${VERSION_SHORT}" "${PROJECT_VERSION}" _plist_contents ${_plist_contents})
    file(WRITE ${_helper_info_plist} ${_plist_contents})

    add_executable(${_helper_target} MACOSX_BUNDLE ${SWR_HELPER_SRCS})
    SET_EXECUTABLE_TARGET_PROPERTIES(${_helper_target})
    add_dependencies(${_helper_target} libcef_dll_wrapper)
    target_link_libraries(${_helper_target} libcef_dll_wrapper ${CEF_STANDARD_LIBS})
    set_target_properties(${_helper_target} PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST ${_helper_info_plist}
      OUTPUT_NAME ${_helper_output_name}
      RUNTIME_OUTPUT_DIRECTORY ${CEF_TARGET_OUT_DIR})

    add_dependencies(${SWR_TARGET} "${_helper_target}")
    add_custom_command(TARGET ${SWR_TARGET} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${CEF_TARGET_OUT_DIR}/${_helper_output_name}.app"
              "${SWR_APP}/Contents/Frameworks/${_helper_output_name}.app"
      VERBATIM)
  endforeach()
endif()
