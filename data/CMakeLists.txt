configure_file(
  "com.ubuntu.developer.rschroll.gmail_gmail-settings.ini"
  "${CMAKE_BINARY_DIR}/src/com.ubuntu.developer.rschroll.gmail_gmail-settings.ini"
)

# Install the scope ini file
install(
  FILES
    "com.ubuntu.developer.rschroll.gmail_gmail.ini"
    "com.ubuntu.developer.rschroll.gmail_gmail-settings.ini"
  DESTINATION ${SCOPE_INSTALL_DIR}
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

