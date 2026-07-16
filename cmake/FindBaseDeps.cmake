# Aseprite
# Copyright (C) 2021-present  Igara Studio S.A.
# Copyright (C) 2001-2018  David Capello

set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")

set(ZLIB_DIR "${THIRD_PARTY_DIR}/zlib")

# zlib
if(USE_SHARED_ZLIB)
  find_package(ZLIB REQUIRED)
else()
  set(SKIP_INSTALL_ALL on)
  # Don't build zlib tests
  set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "Enable Zlib Examples")
  set(ZLIB_ROOT "${THIRD_PARTY_DIR}/zlib" CACHE INTERNAL "")
  add_subdirectory(${THIRD_PARTY_DIR}/zlib)

  set(ZLIB_FOUND ON)
  set(ZLIB_LIBRARY zlibstatic)
  set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
  set(ZLIB_INCLUDE_DIRS
    ${ZLIB_DIR}
    ${CMAKE_BINARY_DIR}/third_party/zlib) # Zlib generated zconf.h file
  set(ZLIB_INCLUDE_DIR ${ZLIB_INCLUDE_DIRS} CACHE PATH "")

  add_library(ZLIB::ZLIB INTERFACE IMPORTED)
  set_target_properties(ZLIB::ZLIB PROPERTIES
    INTERFACE_LINK_LIBRARIES ${ZLIB_LIBRARIES})
endif()
