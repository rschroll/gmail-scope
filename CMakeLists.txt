project(gmail CXX)
cmake_minimum_required(VERSION 2.8.10)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")

# We require g++ 4.9, to avoid ABI breakage with earlier version.
set(cxx_version_required 4.9)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (NOT CMAKE_CXX_COMPILER_VERSION MATCHES "^${cxx_version_required}")
        message(FATAL_ERROR "g++ version must be ${cxx_version_required}!")
    endif()
endif()

# Set strict and naggy C++ compiler flags, and enable C++11
add_definitions(
  -fno-permissive
  -std=c++11
  -pedantic
  -Wall
  -Wextra
  -fPIC
  -DQT_NO_KEYWORDS
)

include(GNUInstallDirs)
find_package(PkgConfig)

# We depend on Boost for string trimming
find_package(
  Boost
  REQUIRED
)

# Search for our dependencies
pkg_check_modules(
  SCOPE
  libunity-scopes>=0.6.0
  net-cpp>=1.1.0
  REQUIRED
)

find_package(Qt5Core REQUIRED)
include_directories(${Qt5Core_INCLUDE_DIRS})

# Add our dependencies to the include paths
include_directories(
  "${CMAKE_SOURCE_DIR}/include"
  ${Boost_INCLUDE_DIRS}
  ${SCOPE_INCLUDE_DIRS}
)

set(SCOPE_INSTALL_NAME "com.ubuntu.developer.rschroll.gmail_gmail")
set(SCOPE_VERSION "0.2.1")

# Do not remove this line, its required for the correct functionality of the Ubuntu-SDK
set(UBUNTU_MANIFEST_PATH "manifest.json.in" CACHE INTERNAL "Tells QtCreator location and name of the manifest file")
set(UBUNTU_PROJECT_TYPE "Scope" CACHE INTERNAL "Tells QtCreator this is a Scope project")

# Important project paths
set(CMAKE_INSTALL_PREFIX /)
set(SCOPE_INSTALL_DIR "/gmail")
set(GETTEXT_PACKAGE "gmail")

# If we need to refer to the scope's name or package in code, these definitions will help
add_definitions(-DPACKAGE_NAME="com.ubuntu.developer.rschroll.gmail")
add_definitions(-DSCOPE_NAME="${SCOPE_INSTALL_NAME}")
add_definitions(-DVERSION="${SCOPE_VERSION}")
add_definitions(-DGETTEXT_PACKAGE="${GETTEXT_PACKAGE}")

#This command figures out the target architecture and puts it into the manifest file
execute_process(
  COMMAND dpkg-architecture -qDEB_HOST_ARCH
  OUTPUT_VARIABLE CLICK_ARCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

file(STRINGS gmail.secret SECRET_STRINGS)
list(GET SECRET_STRINGS 0 CLIENT_ID)
list(GET SECRET_STRINGS 1 CLIENT_SECRET)

configure_file(manifest.json.in ${CMAKE_CURRENT_BINARY_DIR}/manifest.json)
configure_file(gmail.service.in ${CMAKE_CURRENT_BINARY_DIR}/gmail.service)
configure_file(gmail.application.in ${CMAKE_CURRENT_BINARY_DIR}/gmail.application)

# Install the click manifest
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/manifest.json
  "gmail.apparmor"
  ${CMAKE_CURRENT_BINARY_DIR}/gmail.service
  ${CMAKE_CURRENT_BINARY_DIR}/gmail.application
DESTINATION "/")

# Make this file show up in QtCreator
add_custom_target(hidden_files
  ALL
  SOURCES
    manifest.json.in
    "gmail.apparmor"
    gmail.service.in
    gmail.application.in
)

# Add our main directories
add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(po)
