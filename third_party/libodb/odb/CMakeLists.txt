cmake_minimum_required(VERSION 3.3)

set(LIBODB_SOURCES
	exceptions.cxx
	database.cxx
	vector-impl.cxx
	connection.cxx
	prepared-query.cxx
	query-dynamic.cxx
	result.cxx
	schema-catalog.cxx
	section.cxx
	session.cxx
	statement.cxx
	statement-processing.cxx
	tracer.cxx
	transaction.cxx
	details/buffer.cxx
	details/thread.cxx
	details/shared-ptr/base.cxx
)

add_library(libodb STATIC ${LIBODB_SOURCES})

target_include_directories(libodb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(libodb PUBLIC -DLIBODB_STATIC_LIB -DLIBODB_BOOST_STATIC_LIB -DLIBODB_PGSQL_STATIC_LIB)
target_link_libraries(libodb Boost::date_time)
set_target_properties(libodb PROPERTIES FOLDER "third_party")
set_target_properties(libodb PROPERTIES OUTPUT_NAME odb PREFIX "lib" CLEAN_DIRECT_OUTPUT 1)