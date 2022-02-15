ADD_ODB_INTERFACE(ODB_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/include/cmall/db.hpp ${CMAKE_CURRENT_BINARY_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/cmall/version.hpp.in
		${CMAKE_CURRENT_SOURCE_DIR}/include/cmall/version.hpp @ONLY NEWLINE_STYLE LF)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
add_definitions(-DLOGGING_COMPRESS_LOGS)

file(GLOB cmall_SRCS
	include/cmall/scoped_exit.hpp
	include/cmall/time_clock.hpp
	include/cmall/traits-pgsql.hpp
	include/cmall/url_parser.hpp
	include/cmall/version.hpp
	include/cmall/db.hpp
	include/cmall/db_traits.hpp
	include/cmall/internal.hpp
	include/cmall/io_context_pool.hpp
	include/cmall/logging.hpp
	include/cmall/misc.hpp
	include/cmall/cmall.hpp
	include/cmall/database.hpp
	include/cmall/async_connect.hpp
	include/cmall/simple_http.hpp
	include/cmall/socks_proxy.hpp
	include/cmall/url_parser.hpp
	include/cmall/http_last_modified.hpp
	include/cmall/default_cert.hpp
	include/cmall/io.hpp
	include/cmall/handler_type_check.hpp

	src/main.cpp
	src/cmall.cpp
	src/io_context_pool.cpp
	src/internal.cpp
	src/database.cpp
	src/default_cert.cpp
	src/http_last_modified.cpp
)

add_executable(cmall ${cmall_SRCS}
	${ODB_GENERATED}
)

target_include_directories(cmall	PUBLIC
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/include/
	${CMAKE_CURRENT_SOURCE_DIR}/include/cmall
)

if(ENABLE_BUILD_WERROR)
	if(MSVC)
		target_compile_options(cmall PRIVATE /W4 /WX)
	elseif(NOT APPLE AND NOT MINGW)
		if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
			target_compile_options(cmall PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated)
		else()
			target_compile_options(cmall PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated -Wno-deprecated-copy -Wno-error=deprecated-copy -Wno-pessimizing-move)
		endif()
	endif()
endif()

install(TARGETS cmall DESTINATION bin)
