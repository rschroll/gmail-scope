#find_program(INTLTOOL_MERGE intltool-merge)
set(INI_TARGET ${SCOPE_INSTALL_NAME}.ini)
set(SETTINGS_TARGET ${SCOPE_INSTALL_NAME}-settings.ini)

intltool_merge_translations(
  "${CMAKE_CURRENT_SOURCE_DIR}/${INI_TARGET}.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${INI_TARGET}"
  ALL
  UTF8
)

intltool_merge_translations(
  "${CMAKE_CURRENT_SOURCE_DIR}/${SETTINGS_TARGET}.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${SETTINGS_TARGET}"
  ALL
  UTF8
)

# Install the scope ini file
install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${INI_TARGET}
    ${CMAKE_CURRENT_BINARY_DIR}/${SETTINGS_TARGET}
  DESTINATION
    ${SCOPE_INSTALL_DIR}
)

# Install the scope images
install(
  FILES
    "icon-scope.svg"
    "logo.gif"
    "screenshot.png"
  DESTINATION
    "${SCOPE_INSTALL_DIR}"
)

