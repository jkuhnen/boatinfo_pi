# ---------------------------------------------------------------------------
# Frontend2 Author:   jongough aka. Jon Gough Copyright: License:  wxWidgets
# License
# ---------------------------------------------------------------------------
# - Copyright (C)  by Jon Gough                            *
# - License:     GPLv3+
# - This program is free software; you can redistribute it and/or modify  *
# - it under the terms of the GNU General Public License as published by  *
# - the Free Software Foundation; either version 2 of the License, or     *
# - (at your option) any later version.                                   *
# - *
# - This program is distributed in the hope that it will be useful,       *
# - but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# - MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# - GNU General Public License for more details.                          *
# - *
# - You should have received a copy of the GNU General Public License     *
# - along with this program; if not, write to the                         *
# - xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx,                                       *
# - xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx                                       *
# -----------------------------------------------------------------------------
# Frontend2 Author: Jon Gough Testplugin(TP) v1.0.362.0 19/06/2026 DD/MM/YYYY
# format buildCI:yes useODAPI:yes validateJSON:yes
# ---------------------------------------------------------------------------
# -- When changing this file do NOT change the order of the sections.    -- ##
# -- Make changes only between the section blocks identified             -- ##
# -- The whole configuration process relies on this sequence for success.-- ##
#
# -- Modify section below to include all the details for your plugin.    -- ##

set(CMLOC "CMakeLists: ")

cmake_minimum_required(VERSION 3.15)
if(COMMAND cmake_policy)
  if (POLICY CMP0043)
    cmake_policy(SET CMP0043 NEW)
  endif (POLICY CMP0043)
  cmake_policy(SET CMP0048 NEW)
  if (POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
  endif (POLICY CMP0068)
  if (POLICY CMP0076)
    cmake_policy(SET CMP0076 NEW)
  endif (POLICY CMP0076)
  if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
  endif (POLICY CMP0077)
  if (POLICY CMP0079)
    cmake_policy(SET CMP0079 NEW)
  endif (POLICY CMP0079)
  if (POLICY CMP0175)
    cmake_policy(SET CMP0175 NEW)
  endif (POLICY CMP0175)
  if (POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
  endif (POLICY CMP0177)
endif (COMMAND cmake_policy)

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

set(VERBOSE_NAME "BenchyNav")
set(COMMON_NAME "BenchyNav")
set(TITLE_NAME "testplugin")
set(PACKAGE_CONTACT "Johannes Kuhnen")
set(PACKAGE "testplugin")
set(SHORT_DESCRIPTION
    "Benchy vessel information and navigation data"
)
set(LONG_DESCRIPTION
    "Displays Benchy vessel information and navigation data in a docked OpenCPN panel."
)

set(VERSION_MAJOR "1")
set(VERSION_MINOR "0")
set(VERSION_PATCH "372")
set(VERSION_TWEAK "0")
set(VERSION_DATE "14/08/2026") # DD/MM/YYYY format
set(OCPN_MIN_VERSION "ov58")

set(OCPN_API_VERSION_MAJOR "1")
set(OCPN_API_VERSION_MINOR "18")

set(WX_VER "32")
set(WX_VER_PATCH "8")

set(TP_COMMENT "  * Release using CI")

set(PARENT "opencpn")

set(PROD "prod") # Standard Repos
set(BETA "beta") # Standard Repos
set(ALPHA "alpha") # Standard Repos

set(CLOUDSMITH_BASE_REPOSITORY "testplugin") # without the pi
set(CLOUDSMITH_USER "opencpn")

set(XML_INFO_URL
    "https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:opencpn_user_manual:plugins:safety:odraw1.6_pi"
)
set(XML_SUMMARY ${SHORT_DESCRIPTION})
set(XML_DESCRIPTION ${LONG_DESCRIPTION})

option(PLUGIN_USE_SVG "Use SVG graphics" ON)

set(CMAKE_CXX_STANDARD 11)

set(USE_LOCAL_GLU FALSE)
set(OpenGL_GL_PREFERENCE "LEGACY") # not operational

option(USE_GL "Enable OpenGL support" ON)
message(STATUS "${CMLOC}USE_GL: ${USE_GL}")

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE
      "Release"
      CACHE STRING "Choose the type of build" FORCE
  )
endif ("${CMAKE_BUILD_TYPE}" STREQUAL "")
set_property(
  CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "" "Debug" "Release" "RelWithDebInfo"
                                  "MinSizeRel"
)
message(STATUS "${CMLOC}Build type: ${CMAKE_BUILD_TYPE}")

project(${PACKAGE})

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include("PluginSetup")

set(CMAKE_VERBOSE_MAKEFILE "Activate verbose mode for make files" ON)

option(Plugin_CXX11 "Use c++11" OFF)

include("PluginConfigure")

add_definitions(-DUSE_S57)

set(SRCS
    src/testplugin_pi.cpp
)

set(HDRS
    include/testplugin_pi.h
)

add_definitions(-DPLUGIN_USE_SVG)

if (QT_ANDROID)

endif (QT_ANDROID)

set(SRC ${SRCS} ${HDRS})

include_directories(BEFORE ${PROJECT_SOURCE_DIR}/include)
include_directories(BEFORE ${PROJECT_SOURCE_DIR}/ocpninclude)

target_sources(${PACKAGE_NAME} PUBLIC ${SRC})

if (NOT OCPN_FLATPAK_CONFIG)
  message(STATUS "${CMLOC}Adding target link libraries to ${PACKAGE_NAME}")

  if (WIN32)
    add_subdirectory(opencpn-libs/WindowsHeaders)
    target_link_libraries(${PACKAGE_NAME} windows::headers)
  endif (WIN32)

  add_subdirectory(opencpn-libs/api-${OCPN_API_VERSION_MINOR})
  target_link_libraries(${PACKAGE_NAME} ocpn::api)

  add_subdirectory(opencpn-libs/wxJSON)
  target_link_libraries(${PACKAGE_NAME} ocpn::wxjson)

endif (NOT OCPN_FLATPAK_CONFIG)

add_definitions(-DTIXML_USE_STL)

if (UNIX
    AND NOT APPLE
    AND NOT QT_ANDRIOD
    AND NOT OCPN_FLATPAK_CONFIG
    AND NOT OCPN_FLATPAK_BUILD
)
  if (OD_JSON_SCHEMA_VALIDATOR)
    set(lib_name lib${PACKAGE_NAME}.so)
    message(STATUS "${CMLOC}lib-name: ${lib_name}")
    find_file(elfedit elfedit)
    if (NOT (${elfedit} STREQUAL "elfedit-NOTFOUND"))
      message(STATUS "${CMLOC}Will ensure OS/ABI is set to 0 to match OCPN")
      add_custom_command(
        TARGET ${PACKAGE_NAME}
        POST_BUILD
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT " Running post build action on ${lib_name}."
        COMMAND
          sh -c
          'elfedit --input-osabi=Linux --output-osabi=none ${lib_name} || (echo 0) '
      )
    else (NOT (${elfedit} STREQUAL "elfedit-NOTFOUND"))
      message(
        STATUS
          "${CMLOC}Cannot correct OS/ABI to match OpenCPN if it is generated incorrectly. Package binutils needed"
      )
    endif (NOT (${elfedit} STREQUAL "elfedit-NOTFOUND"))
  endif (OD_JSON_SCHEMA_VALIDATOR)
endif (
  UNIX
  AND NOT APPLE
  AND NOT QT_ANDRIOD
  AND NOT OCPN_FLATPAK_CONFIG
  AND NOT OCPN_FLATPAK_BUILD
)

if (QT_ANDROID)
  include_directories(BEFORE ${qt_android_include})
endif (QT_ANDROID)

include("PluginInstall")
include("PluginLocalization")
include("PluginPackage")

message(STATUS "${CMLOC}include directories: ")
get_property(
  dirs
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTY INCLUDE_DIRECTORIES
)
foreach (dir ${dirs})
  message(STATUS "${CMLOC}dir='${dir}'")
endforeach ()
