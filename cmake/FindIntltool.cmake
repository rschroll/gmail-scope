# Copyright (C) 2014 Canonical Ltd
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# This package provides macros that wrap the intltool programs.
# 
# An example of common usage is:
#
# For an ini file:
#
# intltool_merge_translations(
#   "foo.ini.in"
#   "${CMAKE_CURRENT_BINARY_DIR}/foo.ini"
#   ALL
#   UTF8
# )
#
# Inside po/CMakeLists.txt:
#
# intltool_update_potfile(
#     ALL
#     GETTEXT_PACKAGE ${GETTEXT_PACKAGE}
# )
#
# intltool_install_translations(
#     ALL
#     GETTEXT_PACKAGE ${GETTEXT_PACKAGE}
# )

find_package(Gettext REQUIRED)

find_program(INTLTOOL_UPDATE_EXECUTABLE intltool-update)

if(INTLTOOL_UPDATE_EXECUTABLE)
    execute_process(
        COMMAND ${INTLTOOL_UPDATE_EXECUTABLE} --version
        OUTPUT_VARIABLE intltool_update_version
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (intltool_update_version MATCHES "^intltool-update \\(.*\\) [0-9]")
        string(
            REGEX REPLACE "^intltool-update \\([^\\)]*\\) ([0-9\\.]+[^ \n]*).*" "\\1"
            INTLTOOL_UPDATE_VERSION_STRING "${intltool_update_version}"
        )
    endif()
    unset(intltool_update_version)
endif()

find_program(INTLTOOL_MERGE_EXECUTABLE intltool-merge)

if(INTLTOOL_MERGE_EXECUTABLE)
    execute_process(
        COMMAND ${INTLTOOL_MERGE_EXECUTABLE} --version
        OUTPUT_VARIABLE intltool_merge_version
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (intltool_update_version MATCHES "^intltool-merge \\(.*\\) [0-9]")
        string(
            REGEX REPLACE "^intltool-merge \\([^\\)]*\\) ([0-9\\.]+[^ \n]*).*" "\\1"
            INTLTOOL_MERGE_VERSION_STRING "${intltool_merge_version}"
        )
    endif()
    unset(intltool_merge_version)
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    Intltool
    REQUIRED_VARS
      INTLTOOL_UPDATE_EXECUTABLE
      INTLTOOL_MERGE_EXECUTABLE
    VERSION_VAR
      INTLTOOL_UPDATE_VERSION_STRING
    HANDLE_COMPONENTS
)

function(APPEND_EACH LISTNAME GLUE OUTPUT)
    set(_tmp_list "")
    foreach(VAL ${${LISTNAME}})
        list(APPEND _tmp_list "${GLUE}${VAL}")
    endforeach(VAL ${${LISTNAME}})
    set(${OUTPUT} "${_tmp_list}" PARENT_SCOPE)
endfunction()

function(INTLTOOL_UPDATE_POTFILE)
    set(_options ALL)
    set(_oneValueArgs GETTEXT_PACKAGE OUTPUT_FILE PO_DIRECTORY)

    cmake_parse_arguments(_ARG "${_options}" "${_oneValueArgs}" "" ${ARGN})
    
    set(_POT_FILE "${PROJECT}.pot")

    set(_GETTEXT_PACKAGE "")
    if(_ARG_GETTEXT_PACKAGE)
        set(_POT_FILE "${_ARG_GETTEXT_PACKAGE}.pot")
        set(_GETTEXT_PACKAGE --gettext-package="${_ARG_GETTEXT_PACKAGE}")
    endif()

    set(_OUTPUT_FILE "")
    if(_ARG_OUTPUT_FILE)
        set(_POT_FILE "${_ARG_OUTPUT_FILE}")
        set(_OUTPUT_FILE --output-file="${_ARG_OUTPUT_FILE}")
    endif()

    set(_PO_DIRECTORY "${CMAKE_SOURCE_DIR}/po")
    if(_ARG_PO_DIRECTORY)
        set(_PO_DIRECTORY "${_ARG_PO_DIRECTORY}")
    endif()
    
    file(
        GLOB_RECURSE _CODE_SOURCES
        ${CMAKE_SOURCE_DIR}/*.cpp
        ${CMAKE_SOURCE_DIR}/*.cc
        ${CMAKE_SOURCE_DIR}/*.cxx
        ${CMAKE_SOURCE_DIR}/*.vala
        ${CMAKE_SOURCE_DIR}/*.c
        ${CMAKE_SOURCE_DIR}/*.h
        ${CMAKE_SOURCE_DIR}/*.ini.in
    )

    add_custom_command(
        OUTPUT "${_POT_FILE}"
        COMMAND ${INTLTOOL_UPDATE_EXECUTABLE} --pot ${_OUTPUT_FILE} ${_GETTEXT_PACKAGE}
        DEPENDS
          "${_PO_DIRECTORY}/POTFILES.in"
          ${_CODE_SOURCES}
        WORKING_DIRECTORY ${_PO_DIRECTORY}
    )
    
    _GETTEXT_GET_UNIQUE_TARGET_NAME(${_POT_FILE} _UNIQUE_TARGET_NAME)

    if(_ARG_ALL)
        add_custom_target(
          ${_UNIQUE_TARGET_NAME}
          ALL
          DEPENDS
            ${_POT_FILE}
        )
    else()
        add_custom_target(
          ${_UNIQUE_TARGET_NAME}
          DEPENDS
            ${_POT_FILE}
        )
    endif()
endfunction()

function(INTLTOOL_INSTALL_TRANSLATIONS)
    set(_options ALL)
    set(_oneValueArgs GETTEXT_PACKAGE POT_FILE)

    cmake_parse_arguments(_ARG "${_options}" "${_oneValueArgs}" "" ${ARGN})

    set(_POT_FILE "${PROJECT}.pot")

    if(_ARG_GETTEXT_PACKAGE)
        set(_POT_FILE "${_ARG_GETTEXT_PACKAGE}.pot")
    endif()

    if(_ARG_OUTPUT_FILE)
        set(_POT_FILE "${_ARG_OUTPUT_FILE}")
    endif()

    file(
        GLOB _PO_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/*.po
    )

    get_filename_component(_ABS_POT_FILE ${_POT_FILE} ABSOLUTE)

    foreach(_PO_FILE ${_PO_FILES})
        add_custom_command(
           OUTPUT
             ${_PO_FILE}
           COMMAND
             ${GETTEXT_MSGMERGE_EXECUTABLE} --quiet --update --backup=none -s ${_PO_FILE} ${_ABS_POT_FILE}
           DEPENDS
             ${_ABS_POT_FILE}
        )
    endforeach()

    if(_ARG_ALL)
        gettext_create_translations(
          ${_POT_FILE}
          ALL
          ${_PO_FILES}
        )
    else()
        gettext_create_translations(
          ${_POT_FILE}
          ${_PO_FILES}
        )
    endif()
endfunction()

function(INTLTOOL_MERGE_TRANSLATIONS FILENAME OUTPUT_FILE)
    set(_options ALL UTF8 PASS_THROUGH)
    set(_oneValueArgs PO_DIRECTORY)

    cmake_parse_arguments(_ARG "${_options}" "${_oneValueArgs}" "" ${ARGN})

    get_filename_component(_ABS_FILENAME ${FILENAME} ABSOLUTE)

    set(_PO_DIRECTORY "${CMAKE_SOURCE_DIR}/po")
    if(_ARG_PO_DIRECTORY)
        set(_PO_DIRECTORY "${_ARG_PO_DIRECTORY}")
    endif()

    set(_UTF8 "")
    if(_ARG_UTF8)
        set(_UTF8 "--utf8")
    endif()

    set(_PASS_THROUGH "")
    if(_ARG_PASS_THROUGH)
        set(_PASS_THROUGH "--pass-through")
    endif()
    
    file(
        GLOB_RECURSE _PO_FILES
        ${_PO_DIRECTORY}/*.po
    )

    add_custom_command(
        OUTPUT
          ${OUTPUT_FILE}
        COMMAND
          ${INTLTOOL_MERGE_EXECUTABLE} --desktop-style --quiet ${_UTF8} ${_PASS_THROUGH} ${_PO_DIRECTORY} ${FILENAME} ${OUTPUT_FILE}
        DEPENDS
          ${_ABS_FILENAME}
          ${_PO_FILES}
        WORKING_DIRECTORY
          ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    get_filename_component(_OUTPUT_NAME ${OUTPUT_FILE} NAME)
    _GETTEXT_GET_UNIQUE_TARGET_NAME(${_OUTPUT_NAME} _UNIQUE_TARGET_NAME)

    if(_ARG_ALL)
        add_custom_target(
          ${_UNIQUE_TARGET_NAME}
          ALL
          DEPENDS
            ${OUTPUT_FILE}
        )
    else()
        add_custom_target(
          ${_UNIQUE_TARGET_NAME}
          DEPENDS
            ${OUTPUT_FILE}
        )
    endif()
endfunction()
