configure_file(
  "${CMAKE_SOURCE_DIR}/data/logo.gif"
  "${CMAKE_CURRENT_BINARY_DIR}/logo.gif"
  @ONLY
  COPYONLY
)

# The sources to build the scope
set(SCOPE_SOURCES
  api/client.cpp
  scope/preview.cpp
  scope/query.cpp
  scope/scope.cpp
  scope/activation.cpp
  trojita/Encoders.cpp
  trojita/kcodecs.cpp
)

# Find all the headers
file(GLOB_RECURSE
  SCOPE_HEADERS
  "${CMAKE_SOURCE_DIR}/include/*.h" 
)

# Build an object library for the scope code
add_library(
  scope-static OBJECT
  ${SCOPE_SOURCES}
  ${SCOPE_HEADERS}
)

# Ensure we export all the symbols
set_target_properties(
  scope-static
  PROPERTIES
    LINK_FLAGS "-Wl,--export-all-symbols"
)

# Build a shared library containing our scope code.
# This will be the actual plugin that is loaded.
add_library(
  scope SHARED
  $<TARGET_OBJECTS:scope-static>
)

# Link against the object library and our external library dependencies
target_link_libraries(
  scope
  ${SCOPE_LDFLAGS}
  ${Boost_LIBRARIES}
)

qt5_use_modules(
  scope
  Core
)

# Set the correct library output name to conform to the securiry policy 
set_target_properties(
  scope
  PROPERTIES
    OUTPUT_NAME "com.ubuntu.developer.rschroll.gmail_gmail"
)

# Install the scope shared library
install(
  TARGETS scope
  LIBRARY DESTINATION ${SCOPE_INSTALL_DIR}
)

