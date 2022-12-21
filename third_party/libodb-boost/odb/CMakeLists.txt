cmake_minimum_required(VERSION 3.3)

list(APPEND LIBODB_SOURCES
	boost/date-time/exceptions.cxx
)

add_library(libodb-boost STATIC ${LIBODB_SOURCES})

target_include_directories(libodb-boost PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(libodb-boost PUBLIC -DLIBODB_STATIC_LIB -DLIBODB_BOOST_STATIC_LIB -DLIBODB_PGSQL_STATIC_LIB)
target_link_libraries(libodb-boost Boost::date_time)
set_target_properties(libodb-boost PROPERTIES FOLDER "third_party")
set_target_properties(libodb-boost PROPERTIES OUTPUT_NAME odb-boost PREFIX "lib" CLEAN_DIRECT_OUTPUT 1)
