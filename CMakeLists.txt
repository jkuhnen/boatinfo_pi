cmake_minimum_required(VERSION 3.15)
project(boatinfo VERSION 1.1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PACKAGE_NAME "boatinfo_pi")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(OCPN_API_VERSION "1.18")

find_package(wxWidgets REQUIRED COMPONENTS base core aui)
include(${wxWidgets_USE_FILE})

add_library(boatinfo_pi SHARED
    src/boatinfo_pi.cpp
    src/boatinfo_metadata.cpp
    include/boatinfo_pi.h
)

target_include_directories(boatinfo_pi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(opencpn-libs/api-18)
add_subdirectory(opencpn-libs/wxJSON)
target_link_libraries(boatinfo_pi PRIVATE
    ocpn::api
    ocpn::wxjson
    ${wxWidgets_LIBRARIES}
)

if(WIN32)
  add_subdirectory(opencpn-libs/WindowsHeaders)
  target_link_libraries(boatinfo_pi PRIVATE windows::headers)
endif()

if(MSVC)
  target_compile_definitions(boatinfo_pi PRIVATE
      MAKING_PLUGIN
      __MSVC__
      _CRT_NONSTDC_NO_DEPRECATE
      _CRT_SECURE_NO_DEPRECATE
  )
  target_compile_options(boatinfo_pi PRIVATE /utf-8)
endif()

set_target_properties(boatinfo_pi PROPERTIES
    PREFIX ""
    OUTPUT_NAME "boatinfo_pi"
)

if(MSVC)
  set(OCPN_PACKAGE_TARGET "msvc-wx32")
  set(OCPN_BUILD_TARGET "msvc")
else()
  set(OCPN_PACKAGE_TARGET "unknown")
  set(OCPN_BUILD_TARGET "unknown")
endif()

if(CMAKE_VS_PLATFORM_NAME)
  string(TOLOWER "${CMAKE_VS_PLATFORM_NAME}" _platform_name)
  if(_platform_name STREQUAL "win32")
    set(OCPN_TARGET_ARCH "x86")
  elseif(_platform_name STREQUAL "x64")
    set(OCPN_TARGET_ARCH "x86_64")
  elseif(_platform_name STREQUAL "arm64")
    set(OCPN_TARGET_ARCH "arm64")
  else()
    set(OCPN_TARGET_ARCH "${_platform_name}")
  endif()
else()
  set(OCPN_TARGET_ARCH "unknown")
endif()

if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
  set(OCPN_TARGET_VERSION "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
elseif(CMAKE_SYSTEM_VERSION)
  set(OCPN_TARGET_VERSION "${CMAKE_SYSTEM_VERSION}")
else()
  set(OCPN_TARGET_VERSION "unknown")
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/metadata.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/metadata.xml
    @ONLY
)

install(TARGETS boatinfo_pi
    RUNTIME DESTINATION plugins
    LIBRARY DESTINATION plugins
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/metadata.xml DESTINATION .)

set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_NAME "${PACKAGE_NAME}")
set(CPACK_PACKAGE_VERSION "${PACKAGE_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "${PACKAGE_NAME}-${PACKAGE_VERSION}-${OCPN_BUILD_TARGET}-${OCPN_TARGET_ARCH}-${OCPN_TARGET_VERSION}"
)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
