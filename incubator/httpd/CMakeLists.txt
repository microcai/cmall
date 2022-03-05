
file(GLOB httpd_sources
	include/httpd/*.hpp
	src/*.cpp
)
add_library(httpd STATIC ${httpd_sources})

target_include_directories(httpd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(httpd Boost::filesystem)
