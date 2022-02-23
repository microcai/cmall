ADD_ODB_INTERFACE(ODB_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/include/cmall/db.hpp ${CMAKE_CURRENT_BINARY_DIR} --include-prefix cmall)

add_definitions(-DLOGGING_COMPRESS_LOGS)

file(GLOB cmall_SRCS
	include/cmall/async_connect.hpp
	include/cmall/default_cert.hpp
	include/cmall/io.hpp
	include/cmall/logging.hpp
	include/cmall/scoped_exit.hpp
	include/cmall/time_clock.hpp
	include/cmall/url_parser.hpp
	include/cmall/version.hpp
	include/cmall/db.hpp
	include/cmall/db.xml
	include/cmall/database.hpp
	include/cmall/cmall.hpp
	include/cmall/internal.hpp
	include/cmall/js_util.hpp
	include/cmall/misc.hpp
	include/db_traits.hpp
	include/io_context_pool.hpp
	include/traits-pgsql.hpp
	include/bundle_file.hpp
	include/fmt_extra.hpp
	include/jsonrpc.hpp

	src/error_code.cpp
	src/default_cert.cpp
	src/io_context_pool.cpp
	src/main.cpp
	src/cmall.cpp
	src/database.cpp
	src/internal.cpp
)

add_executable(cmall ${cmall_SRCS}
	${ODB_GENERATED}
)
target_include_directories(cmall PRIVATE
	${CMAKE_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/include
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
