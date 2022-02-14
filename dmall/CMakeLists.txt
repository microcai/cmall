ADD_ODB_INTERFACE(ODB_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/include/dmall/db.hpp ${CMAKE_CURRENT_BINARY_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/dmall/version.hpp.in
		${CMAKE_CURRENT_SOURCE_DIR}/include/dmall/version.hpp @ONLY NEWLINE_STYLE LF)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
add_definitions(-DLOGGING_COMPRESS_LOGS)

file(GLOB dmall_SRCS
	include/dmall/scoped_exit.hpp
	include/dmall/time_clock.hpp
	include/dmall/traits-pgsql.hpp
	include/dmall/url_parser.hpp
	include/dmall/version.hpp
	include/dmall/db.hpp
	include/dmall/db_traits.hpp
	include/dmall/internal.hpp
	include/dmall/io_context_pool.hpp
	include/dmall/logging.hpp
	include/dmall/misc.hpp
	include/dmall/dmall.hpp
	include/dmall/database.hpp
	include/dmall/async_connect.hpp
	include/dmall/simple_http.hpp
	include/dmall/socks_proxy.hpp
	include/dmall/url_parser.hpp
	include/dmall/http_last_modified.hpp
	include/dmall/default_cert.hpp
	include/dmall/io.hpp
	include/dmall/handler_type_check.hpp

	src/main.cpp
	src/dmall.cpp
	src/io_context_pool.cpp
	src/internal.cpp
	src/database.cpp
	src/default_cert.cpp
	src/http_last_modified.cpp
)

add_executable(dmall ${dmall_SRCS}
	${ODB_GENERATED}
)

target_include_directories(dmall	PUBLIC
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/include/
	${CMAKE_CURRENT_SOURCE_DIR}/include/dmall
)

if(ENABLE_BUILD_WERROR)
	if(MSVC)
		target_compile_options(dmall PRIVATE /W4 /WX)
	elseif(NOT APPLE AND NOT MINGW)
		if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
			target_compile_options(dmall PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated)
		else()
			target_compile_options(dmall PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated -Wno-deprecated-copy -Wno-error=deprecated-copy -Wno-pessimizing-move)
		endif()
	endif()
endif()

install(TARGETS dmall DESTINATION bin)
