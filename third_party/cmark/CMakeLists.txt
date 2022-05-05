cmake_minimum_required(VERSION 3.7)
project(cmark VERSION 0.30.2)

include(GNUInstallDirs)



option(CMARK_TESTS "Build cmark tests and enable testing" OFF)
option(CMARK_STATIC "Build static libcmark library" ON)
option(CMARK_SHARED "Build shared libcmark library" OFF)
option(CMARK_LIB_FUZZER "Build libFuzzer fuzzing harness" OFF)

if(NOT MSVC)
  set(CMAKE_C_STANDARD 99)
  set(CMAKE_C_STANDARD_REQUIRED YES)
  set(CMAKE_C_EXTENSIONS NO)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# The Linux modules distributed with CMake add "-rdynamic" to the build flags
# which is incompatible with static linking under certain configurations.
# Unsetting CMAKE_SHARED_LIBRARY_LINK_C_FLAGS ensures this does not happen.
if(CMARK_STATIC AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)
endif()

# Check integrity of node structure when compiled as debug
add_compile_options($<$<CONFIG:Debug>:-DCMARK_DEBUG_NODES>)

# In order to maintain compatibility with older platforms which may not have a
# recent version of CMake (i.e. are running CMake <3.3), we cannot simply use
# the `add_compile_options` with a generator expression.  This uses the
# `target_compile_options` with `PRIVATE` to add the flags only to the targets
# so that CMark may be used in projects with non-C languages.
function(cmark_add_compile_options target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /wd4706)
    if(MSVC_VERSION LESS 1800)
      target_compile_options(${target} PRIVATE /TP)
    endif()
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(${target} PRIVATE
            -Wall -Wextra -pedantic
            $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>)
  endif()
endfunction()

add_subdirectory(src)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
  "Choose the type of build, options are: Debug Profile Release Asan Ubsan." FORCE)
endif(NOT CMAKE_BUILD_TYPE)
