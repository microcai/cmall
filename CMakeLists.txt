cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW) # for CMAKE_MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0077 NEW)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
# set(CMAKE_DISABLE_SOURCE_CHANGES ON)

project(cmall)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	if (CMAKE_CXX_SIMULATE_ID MATCHES "MSVC")
		set (MSVC TRUE)
		set (WIN32 TRUE)
		set (CMAKE_SIZEOF_VOID_P 8)
		set (CLANGCL TRUE)
		add_compile_options(/EHs)
		set (CMAKE_SYSTEM_NAME "Windows")
	else()
		set (CLANGCL FALSE)
	endif()
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF) # force use -std=c++20 flag for gcc
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

MESSAGE(STATUS "Set cxx standard: C++20")

string(TIMESTAMP VERSION_YEAR "%Y")
string(TIMESTAMP VERSION_MONTH_YEAR "%m")
string(TIMESTAMP VERSION_DAY_YEAR "%j")

math(EXPR VERSION_MAJOR "${VERSION_YEAR} - 2022")
math(EXPR VERSION_MINOR "${VERSION_MONTH_YEAR}")
math(EXPR VERSION_TINY "${VERSION_DAY_YEAR}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/cmake")

include(CheckCXXCompilerFlag)
include(CheckLibraryExists)
include(CheckCXXSymbolExists)
include(CheckCXXLinkerFlag)

option(USE_BUNDLED_SITE "Build with bundled asserts" ON)

option(ENABLE_SYSTEM_BOOST "Build with system boost support" OFF)

if (WIN32)
	option(ENABLE_BUILD_WERROR "All warnings being treated as errors" OFF)
	option(USE_BUNDLED_ODB_FILE "build using pre generated odb file" ON)
	option(ENABLE_SYSTEM_OPENSSL "Build with system openssl support" OFF)
	option(ENABLE_SYSTEM_ZLIB "Build with system zlib support" OFF)
else()
	option(ENABLE_BUILD_WERROR "All warnings being treated as errors" ON)
	option(USE_BUNDLED_ODB_FILE "build using pre generated odb file" OFF)
	option(ENABLE_SYSTEM_OPENSSL "Build with system openssl support" ON)
	option(ENABLE_SYSTEM_ZLIB "Build with system zlib support" ON)
endif()

option(ENABLE_SYSTEM_GIT2 "Build with system libgit2 support" OFF)
option(ENABLE_TCMALLOC_STATIC "Build with Tcmalloc support" OFF)
option(ENABLE_JEMALLOC_STATIC "Build with Jemalloc support" OFF)
option(ENABLE_LLD "build using lld" OFF)
option(ENABLE_MOLD "build using mold" OFF)
option(ENABLE_IOURING "build with liburing" ON)
option(ENABLE_LTO "enable LTO support" OFF)

find_program(MOLD_LINKER mold)
find_program(LLD_LINKER lld)

if (ENABLE_LTO)
	set(ENABLE_MOLD OFF FORCE)
endif()

if (ENABLE_LLD)
	if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		list(APPEND CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld-link")
	else()
		list(APPEND CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
	endif()
elseif (ENABLE_MOLD)
	list(APPEND CMAKE_EXE_LINKER_FLAGS "-fuse-ld=mold")
else()
	if (MOLD_LINKER)
		if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			list(APPEND CMAKE_EXE_LINKER_FLAGS "-fuse-ld=mold")
		endif()
	elseif(LLD_LINKER)
		list(APPEND CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
	endif()
endif()

if (ENABLE_LTO)
	if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		add_compile_options(-flto=thin)
		add_link_options(-flto=thin)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_compile_options(-flto)
		add_link_options(-flto)
	elseif (MSVC)
		add_compile_options(/GL)
		add_link_options(/LTCG)
	endif()
endif()

CHECK_CXX_COMPILER_FLAG(-fvisibility-inlines-hidden COMPILER_HAS_VISIBILITY_INLINE_HIDDEN)
CHECK_CXX_COMPILER_FLAG(-fvisibility=hidden COMPILER_HAS_VISIBILITY_HIDDEN)
CHECK_CXX_COMPILER_FLAG(-fdiagnostics-color=always COMPILER_HAS_COLOR)
CHECK_CXX_COMPILER_FLAG(-fcoroutines COMPILER_HAS_FCOROUTINES)

if (COMPILER_HAS_VISIBILITY_INLINE_HIDDEN)
	# C 不能使用这个 flag. gcc 会报错
	add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>)
endif()

if (COMPILER_HAS_VISIBILITY_HIDDEN)
	add_compile_options(-fvisibility=hidden)
endif()

if (COMPILER_HAS_COLOR)
	add_compile_options(-fdiagnostics-color=always)
endif()

if (COMPILER_HAS_FCOROUTINES)
	add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fcoroutines>)
endif()

if (WIN32)
	add_definitions(
		-DWIN32_LEAN_AND_MEAN
		-D_WIN32_WINNT=0x0601
		-DNOMINMAX
		-DUNICODE
		-D_UNICODE
		-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
	)
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if (MSVC)
	add_definitions(
		-D_CRT_SECURE_NO_DEPRECATE
		-D_CRT_SECURE_NO_WARNINGS
		-D_CRT_NONSTDC_NO_DEPRECATE
		-D_CRT_NONSTDC_NO_WARNINGS
		-D_SCL_SECURE_NO_DEPRECATE
		-D_SCL_SECURE_NO_WARNINGS
	)

	add_compile_options(/utf-8)
	if (MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

	elseif(MSVC)
		add_compile_options(/IGNORE:4267,4244,4005)
		add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/bigobj>)
		add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>)
		set(CMAKE_CXX_STACK_SIZE "100000000")
		MESSAGE(STATUS "Set stack size: 100000000")
		if (NOT CMAKE_GENERATOR STREQUAL "Ninja")
			MESSAGE(STATUS "Using parallel compiling (/MP)")
			add_compile_options(/MP)
		endif()
	endif()

elseif(WIN32)

elseif (ANDROID)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
else()
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
		add_link_options(-stdlib=libc++)

		CHECK_LINKER_FLAG(CXX "-rtlib=compiler-rt -libunwind=libunwind" IS_COMPILER_RT_SUPPORTED)

		if (IS_COMPILER_RT_SUPPORTED)
			message(STATUS "clang supports compiler-rt, use it")
			add_link_options(-rtlib=compiler-rt -unwindlib=libunwind)
		endif()
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

		CHECK_CXX_COMPILER_FLAG(-fconcepts-diagnostics-depth=99 COMPILER_HAS_fconcepts_diagnostics)

		if (COMPILER_HAS_fconcepts_diagnostics)
			add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fconcepts-diagnostics-depth=99>)
		endif()

	endif()

endif()

check_cxx_symbol_exists(__cpp_lib_format "version" HAS_LIBCPP_FORMAT)

find_package(Threads)
link_libraries(${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

if (ENABLE_SYSTEM_OPENSSL)
	find_package(OpenSSL REQUIRED)
	include_directories(${OPENSSL_INCLUDE_DIR})
else()
	set(OpenSSL_FOUND TRUE)
	add_subdirectory(third_party/openssl)
	set(OPENSSL_CRYPTO_LIBRARY crypto)
	set(OPENSSL_SSL_LIBRARY ssl)
	set(OpenSSL_LIBRARIES crypto ssl)
	if (NOT MSVC AND NOT APPLE)
		if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
			set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--exclude-libs,ALL ${CMAKE_SHARED_LINKER_FLAGS}")
			set(CMAKE_MODULE_LINKER_FLAGS "-Wl,--exclude-libs,ALL ${CMAKE_MODULE_LINKER_FLAGS}")
			set(CMAKE_EXE_LINKER_FLAGS "-Wl,--exclude-libs,ALL ${CMAKE_EXE_LINKER_FLAGS}")
		endif()
	endif()
endif()

find_package(IOUring)

if (IOUring_FOUND AND ENABLE_IOURING)
	message(STATUS "Linux using io_uring...")
	add_definitions(-DBOOST_ASIO_HAS_IO_URING -DBOOST_ASIO_DISABLE_EPOLL)
	link_libraries(${IOUring_LIBRARIES})
endif()

add_definitions(-DBOOST_BEAST_USE_STD_STRING_VIEW)

add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)
add_definitions(-DUSE_STD_STRING_VIEW)
add_definitions(-DBOOST_LOCALE_HIDE_AUTO_PTR)
add_definitions(-DBOOST_DISABLE_PRAGMA_MESSAGE)
add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)

if (ENABLE_SYSTEM_BOOST)
	find_package(Boost 1.81 REQUIRED COMPONENTS thread date_time program_options filesystem regex json timer)
	add_library(boost_asio_static INTERFACE)
	add_library(Boost::asio ALIAS boost_asio_static)
	add_library(boost_beast_static INTERFACE)
	add_library(Boost::beast ALIAS boost_beast_static)
else()
	set(Boost_USE_STATIC_LIBS ON)
	set(Boost_USE_STATIC_RUNTIME ON)

	set(Boost_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost)

	add_subdirectory(third_party/boost EXCLUDE_FROM_ALL)
endif()

if (ENABLE_TCMALLOC_STATIC)
	find_package(Tcmalloc)
	link_libraries(${Tcmalloc_LIBRARY})
endif()

if (ENABLE_JEMALLOC_STATIC)
	find_package(JeMalloc)
	link_libraries(${JEMALLOC_LIBRARIES})
endif()

if (WIN32 OR NOT ENABLE_SYSTEM_ZLIB)
	add_subdirectory(third_party/zlib EXCLUDE_FROM_ALL)
	set(ZLIB_LIBRARIES zlib)
	set(ZLIB_LIBRARY zlib)
	set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib)
	set(ZLIB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib)

	add_subdirectory(third_party/zlib/contrib EXCLUDE_FROM_ALL)
	set(MINIZIP_LIBRARIES minizip)
	set(MINIZIP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib/contrib)
	set(ZLIB_FOUND TRUE)
else()
	find_package(ZLIB REQUIRED)
	find_package(MiniZip REQUIRED)
endif()

if(NOT HAS_LIBCPP_FORMAT)
add_subdirectory(third_party/fmt)
endif()

add_subdirectory(third_party/libpq)
set(PostgreSQL_LIBRARIES libpq)

if (NOT USE_BUNDLED_ODB_FILE)
	if(WIN32)
		set(ODB_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-x86_64-windows/bin/odb.exe)
		set(ODB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-x86_64-windows/include)
	elseif(APPLE)
		set(USE_BUNDLED_ODB_FILE TRUE)
	else()
		if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "mips64")
			set(USE_BUNDLED_ODB_FILE TRUE)
		elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64")
			set(USE_BUNDLED_ODB_FILE TRUE)
		else()
			set(ODB_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-x86_64-linux-gnu/bin/odb)
			set(ODB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-x86_64-linux-gnu/include)
		endif()
	endif()
endif()

set(ODB_PGSQL_LIB_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libodb-pgsql)
set(ODB_BOOST_LIB_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libodb-boost)
set(ODB_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libodb)

find_package(odb)

include(cmake/FileGroup.cmake)

include_directories(third_party
	${ZLIB_INCLUDE_DIRS}
	${MINIZIP_INCLUDE_DIRS}
	${PostgreSQL_INCLUDE_DIRS}
	${ODB_INCLUDE_DIRS}
	${ODB_LIB_DIR}
	${ODB_BOOST_LIB_DIR}
	${ODB_PGSQL_LIB_DIR}
)

add_subdirectory(third_party/libodb)
add_subdirectory(third_party/libodb-pgsql)
add_subdirectory(third_party/libodb-boost)

if (ENABLE_SYSTEM_GIT2)
	find_package(libgit2 REQUIRED)
	include_directories(${LIBGIT2_INCLUDE_DIR})
else()
	set(BUILD_SHARED_LIBS OFF)
	set(BUILD_TESTS OFF)
	set(USE_NSEC OFF)
	set(USE_SSH OFF)
	set(SONAME OFF)
	set(USE_NTLMCLIENT OFF)
	set(USE_ICONV OFF)
	set(USE_BUNDLED_ZLIB OFF)
	set(REGEX_BACKEND "builtin")
	set(USE_HTTPS OFF)
	set(USE_THREADS ON)
	set(USE_HTTP_PARSER "builtin")
	set(USE_SHA1 "CollisionDetection")
	set(DEPRECATE_HARD ON)
	set(STATIC_CRT ON)
	set(USE_NTLMCLIENT OFF)
	add_subdirectory(third_party/libgit2 EXCLUDE_FROM_ALL)

	set_target_properties(git2 PROPERTIES FOLDER "third_party/")
endif()

add_subdirectory(third_party/cmark EXCLUDE_FROM_ALL)

set(MAGIC_ENUM_OPT_INSTALL OFF)
add_subdirectory(third_party/magic_enum)

link_libraries(
		Boost::thread
		Boost::date_time
		Boost::program_options
		Boost::regex
		Boost::json
		Boost::timer
		Boost::asio
		Boost::beast
		Boost::filesystem

		libodb
		libodb-pgsql
		libodb-boost

		cmark_static
		magic_enum

		${OPENSSL_LIBRARIES}
		${PostgreSQL_LIBRARIES}
		${ZLIB_LIBRARIES}
		${MINIZIP_LIBRARIES}
)

if(NOT HAS_LIBCPP_FORMAT)
	link_libraries(fmt)
endif()

if (WIN32)
	link_libraries(
		secur32.lib
		bcrypt.lib
		winmm.lib
		mswsock.lib
	)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (WIN32)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_LOGGER -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")

add_subdirectory(incubator)
add_subdirectory(cmall)

configure_file(cmall.service.in cmall.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cmall.service DESTINATION /usr/lib/systemd/system/)

add_subdirectory(misc-debug)

add_subdirectory(test)
