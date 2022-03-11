
if (USE_BUNDLED_ODB_FILE)
	set(ODB_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/../odb_generated/cmall/db-odb.cxx)
else()
	ADD_ODB_INTERFACE(ODB_GENERATED ${CMAKE_CURRENT_SOURCE_DIR}/include/cmall/db.hpp ${CMAKE_CURRENT_SOURCE_DIR}/../odb_generated --include-prefix cmall)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../odb_generated)
add_definitions(-DLOGGING_COMPRESS_LOGS)

file(GLOB cmall_SRCS
	LIST_DIRECTORIES false
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	CONFIGURE_DEPENDS

	include/cmall/*.hpp
	include/*.hpp
	include/utils/*.hpp

	src/*.cpp
	src/services/*

)

find_package(Embed REQUIRED)
EMBED_TARGET(SITE site.zip)

GOURP_FILES(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(cmall ${cmall_SRCS}
	${ODB_GENERATED}
	${EMBED_SITE_OUTPUTS}
)

target_link_libraries(cmall gitpp httpd)

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
			target_compile_options(cmall PRIVATE -Werror -Wall -Wno-unknown-pragmas -Wno-deprecated -Wno-unused-parameter -Wno-unused-variable)
		else()
			target_compile_options(cmall PRIVATE -Werror -Wall -Wno-unknown-pragmas -Wno-deprecated -Wno-unused-parameter -Wno-unused-variable -Wno-deprecated-copy -Wno-error=deprecated-copy -Wno-pessimizing-move)
		endif()
	endif()
endif()

install(TARGETS cmall DESTINATION bin)
