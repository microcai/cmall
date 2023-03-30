
if (USE_BUNDLED_ODB_FILE)
	set(ODB_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/../odb_generated/cmall/db-odb.cxx)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../odb_generated)
elseif(MSVC)
	ADD_ODB_INTERFACE(ODB_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/include/cmall/db.hpp ${CMAKE_CURRENT_BINARY_DIR}/cmall --include-prefix cmall)
	include_directories(${CMAKE_CURRENT_BINARY_DIR})
else()
	ADD_ODB_INTERFACE(ODB_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/include/cmall/db.hpp ${CMAKE_CURRENT_SOURCE_DIR}/../odb_generated/cmall --include-prefix cmall)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../odb_generated)
endif()

add_definitions(-DLOGGING_COMPRESS_LOGS)

find_package(seccomp)

file(GLOB cmall_SRCS
	LIST_DIRECTORIES false
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	CONFIGURE_DEPENDS

	include/cmall/*.hpp
	include/*.hpp
	include/utils/*.hpp
	include/services/*.hpp

	src/*.cpp
	src/services/*
	src/cmall_impl/*

)

find_package(Embed REQUIRED)
EMBED_TARGET(SITE site.zip)

GOURP_FILES(${CMAKE_CURRENT_SOURCE_DIR})

list(FILTER cmall_SRCS EXCLUDE REGEX src/main.cpp)

add_library(cmall_service STATIC ${cmall_SRCS}
	${ODB_GENERATED}
	${EMBED_SITE_OUTPUTS}
)

target_link_libraries(cmall_service gitpp httpd dirmon)


target_precompile_headers(cmall_service PUBLIC
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/stdafx.hpp>"
)

if (SECCOMP_FOUND)
	target_link_libraries(cmall_service ${SECCOMP_LIBRARY})
endif()


target_include_directories(cmall_service PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(cmall_service PRIVATE
	${CMAKE_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(cmall src/main.cpp)
target_link_libraries(cmall cmall_service Boost::asio)

if(ENABLE_BUILD_WERROR)
	if(MSVC)
	target_compile_options(cmall PRIVATE /W4 /WX)
	target_compile_options(cmall_service PRIVATE /W4 /WX)
	elseif(NOT APPLE AND NOT MINGW)
		if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
			target_compile_options(cmall_service PRIVATE -Werror -Wall -Wno-unknown-pragmas -Wno-deprecated -Wno-unused-parameter -Wno-unused-variable)
		else()
			target_compile_options(cmall_service PRIVATE -Werror -Wall -Wno-unknown-pragmas -Wno-deprecated -Wno-unused-parameter -Wno-unused-variable -Wno-deprecated-copy -Wno-error=deprecated-copy -Wno-pessimizing-move)
		endif()
	endif()
endif()

install(TARGETS cmall DESTINATION bin)
