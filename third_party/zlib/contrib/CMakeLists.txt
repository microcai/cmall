cmake_minimum_required(VERSION 3.8)
project(minizip C)

file(GLOB MINIZIPLIB_SRC unzip.c unzip.h)

add_library(minizip STATIC minizip/unzip.c minizip/unzip.h minizip/ioapi.h minizip/ioapi.c)

target_include_directories(minizip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(minizip PUBLIC ${ZLIB_INCLUDE_DIRS})
set_target_properties(minizip PROPERTIES FOLDER "third_party")
