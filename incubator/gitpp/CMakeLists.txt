

add_library(gitpp STATIC src/gitpp.cpp)

target_include_directories(gitpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(gitpp git2 Boost::filesystem)
