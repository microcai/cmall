cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0091 NEW) # for CMAKE_MSVC_RUNTIME_LIBRARY
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(cmall)

string(TIMESTAMP VERSION_YEAR "%Y")
string(TIMESTAMP VERSION_MONTH_YEAR "%m")
string(TIMESTAMP VERSION_DAY_YEAR "%j")

math(EXPR VERSION_MAJOR "${VERSION_YEAR} - 2018")
math(EXPR VERSION_MINOR "${VERSION_MONTH_YEAR}")
math(EXPR VERSION_TINY "${VERSION_DAY_YEAR}")

include(CheckCXXCompilerFlag)
include(CheckLibraryExists)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(ENABLE_BUILD_WERROR "All warnings being treated as errors" OFF)

option(ENABLE_SYSTEM_OPENSSL "Build with system openssl support" OFF)
option(ENABLE_SYSTEM_ZLIB "Build with system zlib support" OFF)
option(ENABLE_SYSTEM_BOOST "Build with system boost support" OFF)

option(ENABLE_TCMALLOC_STATIC "Build with Tcmalloc support" OFF)
option(ENABLE_JEMALLOC_STATIC "Build with Jemalloc support" OFF)

option(ENABLE_GIT_VERSION "Enable git version" OFF)

option(ENABLE_IOURING "Enable iouring" OFF)

if (ENABLE_GIT_VERSION)
	find_package(Git)
	gitGetVersion(${CMAKE_CURRENT_SOURCE_DIR} cmall)
	set(VERSION_GIT ${cmall_WC_REVISION_HASH})
	string(TIMESTAMP PVERSION "%Y%m%d-${VERSION_GIT}-${CMAKE_SYSTEM_NAME}")
endif()

set(CMAKE_C_STANDARD 17)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

	add_definitions(
		-D_CRT_SECURE_NO_DEPRECATE
		-D_CRT_SECURE_NO_WARNINGS
		-D_CRT_NONSTDC_NO_DEPRECATE
		-D_CRT_NONSTDC_NO_WARNINGS
		-D_SCL_SECURE_NO_DEPRECATE
		-D_SCL_SECURE_NO_WARNINGS
	)

	add_definitions(
		-DWIN32_LEAN_AND_MEAN
		-D_WIN32_WINNT=0x0601
		-DNOMINMAX
		-DUNICODE
		-D_UNICODE
		-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
	)

	add_definitions(
		-DBOOST_ALL_STATIC_LINK
		-DBOOST_THREAD_USE_LIB
		-DBOOST_FILESYSTEM_STATIC_LINK
		-DBOOST_USE_WINAPI_VERSION=0x0601
	)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj /MP")
	MESSAGE(STATUS "Using parallel compiling (/MP)")
	set(CMAKE_CXX_STACK_SIZE "100000000") #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:100000000")
	MESSAGE(STATUS "Set stack size: 100000000")

	# if (MSVC_VERSION GREATER_EQUAL "1900")
	# 	CHECK_CXX_COMPILER_FLAG("/std:c++20" HAS_CPP_LATEST_FLAG)
	# 	if (HAS_CPP_LATEST_FLAG)
	# 		set(CMAKE_CXX_STANDARD 20)
	# 		set(CMAKE_CXX_STANDARD_REQUIRED ON)
	# 		MESSAGE(STATUS "Set cxx standard: C++20")
	# 	endif()
	# endif()
elseif(WIN32)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

	add_definitions(
		-DWIN32_LEAN_AND_MEAN
		-D_WIN32_WINNT=0x0601
		-DNOMINMAX
		-DUNICODE
		-D_UNICODE
		-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
	)

	add_definitions(
		-DBOOST_ALL_STATIC_LINK
		-DBOOST_THREAD_USE_LIB
		-DBOOST_FILESYSTEM_STATIC_LINK
		-DBOOST_USE_WINAPI_VERSION=0x0601
	)
endif()

CHECK_CXX_COMPILER_FLAG(-fvisibility-inlines-hidden COMPILER_HAS_VISIBILITY_INLINE_HIDDEN)
CHECK_CXX_COMPILER_FLAG(-fvisibility=hidden COMPILER_HAS_VISIBILITY_HIDDEN)
# CHECK_CXX_COMPILER_FLAG(-std=c++14 LIBCXX_HAS_STDCXX14_FLAG)
# CHECK_CXX_COMPILER_FLAG(-std=c++17 LIBCXX_HAS_STDCXX17_FLAG)
CHECK_CXX_COMPILER_FLAG(-fdiagnostics-color=always COMPILER_HAS_COLOR)
CHECK_CXX_COMPILER_FLAG(-std=c++20 LIBCXX_HAS_STDCXX20_FLAG)
CHECK_CXX_COMPILER_FLAG(-fcoroutines SUPPORT_CXX_COROUTINES)
CHECK_CXX_COMPILER_FLAG(-fcoroutines-ts SUPPORT_CXX_COROUTINES_TS)

if (${COMPILER_HAS_VISIBILITY_INLINE_HIDDEN})
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
endif()

if (${COMPILER_HAS_VISIBILITY_HIDDEN})
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()

if (${COMPILER_HAS_COLOR})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
endif()

if (${SUPPORT_CXX_COROUTINES})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
	message(STATUS "use -fcoroutines")
endif()
if (${SUPPORT_CXX_COROUTINES_TS})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines_ts -stdlib=libc++")
	message(STATUS "use -fcoroutines-ts -stdlib=libc++")
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -O3 -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	# using Clang
	add_definitions(-DBOOST_ASIO_HAS_CO_AWAIT)
	# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fcoroutines-ts")
	# list(APPEND CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	# using GCC
	# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	# using Visual Studio C++
endif()

if (NOT MSVC)
	if(NOT APPLE)
		if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND WIN32)
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
		else()
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
		endif()
	endif()
	if (ANDROID)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fPIE -pie")
	endif()
	if(LIBCXX_HAS_STDCXX20_FLAG)
		# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -fcoroutines")
		add_definitions(-DUSE_STD_STRING_VIEW)
	else()
		message(FATAL_ERROR "need at least GCC 10 or clang 8")
	endif()
endif(NOT MSVC)

#add_definitions(-DJSONRPC_DISABLE_THREADS)
# add_subdirectory(third_party/crypto)

if (ENABLE_SYSTEM_BOOST)
	set(Boost_USE_STATIC_RUNTIME ON)
	find_package(Boost 1.77 REQUIRED COMPONENTS system json date_time regex context coroutine thread filesystem program_options)
else()
	set(Boost_USE_STATIC_LIBS ON)
	set(Boost_USE_STATIC_RUNTIME ON)

	set(Boost_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost)

	add_definitions(-DBOOST_LOCALE_HIDE_AUTO_PTR)
	add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)
	add_definitions(-DBOOST_DISABLE_PRAGMA_MESSAGE)
	add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)
	add_subdirectory(third_party/boost)
endif()

find_package(Threads)
link_libraries(${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

if (ENABLE_TCMALLOC_STATIC)
	find_package(Tcmalloc)
	link_libraries(${Tcmalloc_LIBRARY})
endif()

if (ENABLE_JEMALLOC_STATIC)
	find_package(JeMalloc)
	link_libraries(${JEMALLOC_LIBRARIES})
endif()

if (ENABLE_IOURING)
	find_package(iouring REQUIRED)
	add_definitions(-DBOOST_ASIO_HAS_IO_URING -DBOOST_ASIO_DISABLE_EPOLL)
	link_libraries(${iouring_LIBRARIES})
endif()

if (WIN32 OR NOT ENABLE_SYSTEM_ZLIB)
	add_subdirectory(third_party/zlib EXCLUDE_FROM_ALL)
	set(ZLIB_LIBRARIES zlib)
	set(ZLIB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib)
else()
	find_package(ZLIB REQUIRED)
	find_package(minizip REQUIRED)
endif()

add_subdirectory(third_party/fmt)

if (WIN32)
	add_subdirectory(third_party/libpq)
	set(PostgreSQL_LIBRARIES libpq)
	set(PostgreSQL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libpq/include)
else()
	find_package(PostgreSQL REQUIRED)
endif()

if(WIN32)
	set(ODB_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-x86_64-windows/bin/odb.exe)
	set(ODB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-x86_64-windows/include)
elseif(APPLE)
	set(ODB_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-x86_64-darwin-gnu/bin/odb)
	set(ODB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-x86_64-darwin-gnu/include)
else()
	if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "mips64")
		set(ODB_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-mips64el-linux-gnu/bin/odb)
		set(ODB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-mips64el-linux-gnu/include)
	elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64")
		set(ODB_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-arm64-linux-gnu/bin/odb)
		set(ODB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-arm64-linux-gnu/include)
	else()
		set(ODB_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-x86_64-linux-gnu/bin/odb)
		set(ODB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/odb-2.5.0-x86_64-linux-gnu/include)
	endif()
endif()

set(ODB_PGSQL_LIB_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libodb-pgsql)
set(ODB_BOOST_LIB_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libodb-boost)
set(ODB_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libodb)
include(cmake/odb.cmake)

include_directories(third_party
	${ZLIB_INCLUDE_DIRS}
	${PostgreSQL_INCLUDE_DIRS}
	${ODB_INCLUDE_DIRS}
	${ODB_LIB_DIR}
	${ODB_BOOST_LIB_DIR}
	${ODB_PGSQL_LIB_DIR}
)

add_subdirectory(third_party/libodb)
add_subdirectory(third_party/libodb-pgsql)
add_subdirectory(third_party/libodb-boost)

if (ENABLE_SYSTEM_OPENSSL)
	find_package(OpenSSL)
else()
	add_subdirectory(third_party/openssl)
	if (NOT MSVC AND NOT APPLE)
		if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
			set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--exclude-libs,ALL ${CMAKE_SHARED_LINKER_FLAGS}")
			set(CMAKE_MODULE_LINKER_FLAGS "-Wl,--exclude-libs,ALL ${CMAKE_MODULE_LINKER_FLAGS}")
			set(CMAKE_EXE_LINKER_FLAGS "-Wl,--exclude-libs,ALL ${CMAKE_EXE_LINKER_FLAGS}")
		endif()
	endif()
endif()
include_directories(${OPENSSL_INCLUDE_DIR})

# set(FLATBUFFERS_BUILD_TESTS OFF)
# set(FLATBUFFERS_INSTALL OFF)
# set(FLATBUFFERS_BUILD_FLATHASH OFF)
# set(FLATBUFFERS_BUILD_FLATC OFF)
# set(FLATBUFFERS_BUILD_CPP17 ON)
# set(FLATBUFFERS_LIBCXX_WITH_CLANG OFF)
# set(FLATBUFFERS_INSTALL OFF)
# add_subdirectory(third_party/flatbuffers EXCLUDE_FROM_ALL)
# set(FLATBUFFERS_INCLUDE_DIRS third_party/flatbuffers/include)

# set_target_properties(flatbuffers PROPERTIES FOLDER "third_party")
# set_target_properties(flatbuffers PROPERTIES POSITION_INDEPENDENT_CODE ON)

link_libraries(
		Boost::coroutine
		Boost::context
		Boost::thread
		Boost::date_time
		Boost::filesystem
		Boost::program_options
		Boost::regex
		Boost::json

		# flatbuffers

		fmt
		libodb
		libodb-pgsql
		libodb-boost

		${OPENSSL_LIBRARIES}
		${PostgreSQL_LIBRARIES}
		${ZLIB_LIBRARIES}
)

if (WIN32)
	link_libraries(
		Secur32.lib
		Bcrypt.lib
		Winmm.lib
		Mswsock.lib
	)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (WIN32)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/release)
endif()

add_subdirectory(cmall)

configure_file(cmall.service.in cmall.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cmall.service DESTINATION /usr/lib/systemd/system/)
install(TARGETS cmall DESTINATION bin)
