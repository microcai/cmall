if(${CMAKE_VERSION} VERSION_GREATER "3.3")
    cmake_policy(SET CMP0063 NEW)
endif()

set(LIBRARY "cmark")
set(STATICLIBRARY "cmark_static")
set(HEADERS
  cmark.h
  parser.h
  buffer.h
  node.h
  iterator.h
  chunk.h
  references.h
  utf8.h
  scanners.h
  inlines.h
  houdini.h
  cmark_ctype.h
  render.h
  )
set(LIBRARY_SOURCES
  cmark.c
  node.c
  iterator.c
  blocks.c
  inlines.c
  scanners.c
  scanners.re
  utf8.c
  buffer.c
  references.c
  render.c
  man.c
  xml.c
  html.c
  commonmark.c
  latex.c
  houdini_href_e.c
  houdini_html_e.c
  houdini_html_u.c
  cmark_ctype.c
  ${HEADERS}
  )

set(PROGRAM "cmark_exe")
set(PROGRAM_SOURCES main.c)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmark_version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/cmark_version.h)

include(GNUInstallDirs)
include (GenerateExportHeader)

# -fvisibility=hidden
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

if (CMARK_SHARED)
  add_library(${LIBRARY} SHARED ${LIBRARY_SOURCES})
  cmark_add_compile_options(${LIBRARY})
  set_target_properties(${LIBRARY} PROPERTIES
    MACOSX_RPATH TRUE
    OUTPUT_NAME "cmark"
    # Avoid name clash between PROGRAM and LIBRARY pdb files.
    PDB_NAME cmark_dll
    # Include minor version and patch level in soname for now.
    SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
    VERSION ${PROJECT_VERSION})
  target_include_directories(${LIBRARY} INTERFACE
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
  add_library(cmark::cmark ALIAS ${LIBRARY})

  generate_export_header(${LIBRARY}
    BASE_NAME ${PROJECT_NAME})

  list(APPEND CMARK_INSTALL ${LIBRARY})
endif()

if (CMARK_STATIC)
  add_library(${STATICLIBRARY} STATIC ${LIBRARY_SOURCES})
  cmark_add_compile_options(${STATICLIBRARY})
  set_target_properties(${STATICLIBRARY} PROPERTIES
    COMPILE_FLAGS -DCMARK_STATIC_DEFINE
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION})
  if(MSVC)
    set_target_properties(${STATICLIBRARY} PROPERTIES
      OUTPUT_NAME cmark_static)
  else()
    set_target_properties(${STATICLIBRARY} PROPERTIES
      OUTPUT_NAME cmark)
  endif()
  target_include_directories(${STATICLIBRARY} INTERFACE
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
  add_library(cmark::cmark_static ALIAS ${STATICLIBRARY})

  if (NOT CMARK_SHARED)
    generate_export_header(${STATICLIBRARY}
      BASE_NAME ${PROJECT_NAME})
  endif()

  list(APPEND CMARK_INSTALL ${STATICLIBRARY})
endif()

if(NOT MSVC OR CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS ON)
  include(InstallRequiredSystemLibraries)
endif()

# Feature tests
include(CheckIncludeFile)
include(CheckCSourceCompiles)
CHECK_INCLUDE_FILE(stdbool.h HAVE_STDBOOL_H)
CHECK_C_SOURCE_COMPILES(
  "int main() { __builtin_expect(0,0); return 0; }"
  HAVE___BUILTIN_EXPECT)
CHECK_C_SOURCE_COMPILES("
  int f(void) __attribute__ (());
  int main() { return 0; }
" HAVE___ATTRIBUTE__)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/config.h)

if(CMARK_LIB_FUZZER)
  add_executable(cmark-fuzz ../test/cmark-fuzz.c ${LIBRARY_SOURCES})
  cmark_add_compile_options(cmark-fuzz)
  target_link_libraries(cmark-fuzz "${CMAKE_LIB_FUZZER_PATH}")

  # cmark is written in C but the libFuzzer runtime is written in C++ which
  # needs to link against the C++ runtime.
  set_target_properties(cmark-fuzz PROPERTIES
    LINKER_LANGUAGE CXX)
endif()
