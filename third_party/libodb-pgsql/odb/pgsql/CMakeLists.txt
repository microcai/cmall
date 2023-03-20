cmake_minimum_required(VERSION 3.3)

add_definitions(-DLIBODB_PGSQL_STATIC_BUILD)

set(LIBODB_PGSQL_SOURCES
	auto-handle.cxx
	connection.cxx
	connection-factory.cxx
	database.cxx
	error.cxx
	exceptions.cxx
	prepared-query.cxx
	query.cxx
	query-dynamic.cxx
	query-const-expr.cxx
	simple-object-statements.cxx
	statement.cxx
	statements-base.cxx
	tracer.cxx
	traits.cxx
	transaction.cxx
	transaction-impl.cxx
	details/endian-traits.cxx
	details/options.cxx
)

add_library(libodb-pgsql STATIC ${LIBODB_PGSQL_SOURCES})
target_include_directories(libodb-pgsql PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_compile_definitions(libodb-pgsql PUBLIC -DLIBODB_STATIC_LIB -DLIBODB_BOOST_STATIC_LIB -DLIBODB_PGSQL_STATIC_LIB)
target_link_libraries(libodb-pgsql libodb)
set_target_properties(libodb-pgsql PROPERTIES FOLDER "third_party")
set_target_properties(libodb-pgsql PROPERTIES OUTPUT_NAME odb-pgsql PREFIX "lib" CLEAN_DIRECT_OUTPUT 1)
target_link_libraries(libodb-pgsql ${PostgreSQL_LIBRARIES})