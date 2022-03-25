
include(CheckSymbolExists)
include(CheckFunctionExists)
include(AlignOf.cmake)
include(CheckTypeSize)
include(CheckIncludeFile)
include(CheckVariableExists)
include(CheckCSourceCompiles)

find_package(Threads)

ALIGNOF(double C DOUBLE)

ALIGNOF(short C SHORT)
ALIGNOF("long long" C LONG_LONG_INT)
ALIGNOF(long C LONG)
ALIGNOF(int C INT)

check_type_size("__int128_t" SIZE_OF_INT128)

if (SIZE_OF_INT128)
	ALIGNOF(__int128_t C PG_INT128_TYPE)
else()
	check_type_size("__int128" SIZE_OF__INT128)
	if (SIZE_OF__INT128)
		ALIGNOF(__int128 C PG_INT128_TYPE)
	endif()
endif()

set(BLCKSZ 8192)

check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)
check_function_exists(fdatasync HAVE_DECL_FDATASYNC)
check_function_exists(posix_fadvise HAVE_DECL_POSIX_FADVISE)
check_function_exists(preadv HAVE_DECL_PREADV)
check_function_exists(pwritev HAVE_DECL_PWRITEV)
check_function_exists(strlcat HAVE_DECL_STRLCAT)
check_function_exists(strlcpy HAVE_DECL_STRLCPY)
check_function_exists(strnlen HAVE_DECL_STRNLEN)
check_function_exists(strtoll HAVE_DECL_STRTOLL)
check_function_exists(strtoull HAVE_DECL_STRTOULL)
check_function_exists(dlopen HAVE_DLOPEN)
check_function_exists(fseeko HAVE_FSEEKO)
if (WIN32)
set(HAVE_GETADDRINFO 1)
else()
check_function_exists(getaddrinfo HAVE_GETADDRINFO)
endif()
check_function_exists(gethostbyname_r HAVE_GETHOSTBYNAME_R)
check_function_exists(getifaddrs HAVE_GETIFADDRS)
check_function_exists(getpeereid HAVE_GETPEEREID)
check_function_exists(getpeerucred HAVE_GETPEERUCRED)
check_function_exists(getrusage HAVE_GETRUSAGE)
check_function_exists(getrlimit HAVE_GETRLIMIT)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(memset_s HAVE_MEMSET_S)
check_function_exists(kqueue HAVE_KQUEUE)
check_function_exists(mkdtemp HAVE_MKDTEMP)
check_function_exists(poll HAVE_POLL)
check_function_exists(inet_aton HAVE_INET_ATON)
check_function_exists(posix_fadvise HAVE_POSIX_FADVISE)
check_function_exists(posix_fallocate HAVE_POSIX_FALLOCATE)
check_function_exists(ppoll HAVE_PPOLL)
check_function_exists(pread HAVE_PREAD)
check_function_exists(pwrite HAVE_PWRITE)
check_function_exists(readv HAVE_READV)
check_function_exists(shm_open HAVE_SHM_OPEN)
check_function_exists(srandom HAVE_SRANDOM)
check_function_exists(random HAVE_RANDOM)
check_function_exists(pstat HAVE_PSTAT)
check_function_exists(strerror_r HAVE_STRERROR_R)
check_function_exists(strchrnul HAVE_STRCHRNUL)
check_function_exists(strsignal HAVE_STRSIGNAL)
check_function_exists(explicit_bzero HAVE_EXPLICIT_BZERO)

set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

check_function_exists(pthread_barrier_wait HAVE_PTHREAD_BARRIER_WAIT)

check_variable_exists(timezone HAVE_INT_TIMEZONE)

if (WIN32)
set(HAVE_STRUCT_SOCKADDR_STORAGE 1)
else()
check_c_source_compiles("#include <sys/socket.h>\nstruct sockaddr_storage * abc(){ return 0;} int main(){return 0;}" HAVE_STRUCT_SOCKADDR_STORAGE )
endif()
if (WIN32)
	set(HAVE_STRUCT_ADDRINFO 1)
else()
	check_c_source_compiles("#include <netdb.h>\nstruct addrinfo * abc(){ return 0;} int main(){return 0;}" HAVE_STRUCT_ADDRINFO )
endif()

check_c_source_compiles("int main(){ __func__ ;return 0;}" HAVE_FUNCNAME__FUNC )
check_c_source_compiles("int main(){ __FUNCTION__ ;return 0;}" HAVE_FUNCNAME__FUNCTION )
check_c_source_compiles("int main(){ __atomic_thread_fence(__ATOMIC_SEQ_CST) ;return 0;}" HAVE_GCC__ATOMIC_INT32_CAS )
check_c_source_compiles("#include <stdlib.h> int main(){ int64_t a,b,c; __atomic_compare_exchange_n(&a, &b, c) ;return 0;}" HAVE_GCC__ATOMIC_INT64_CAS )

check_c_source_compiles("int main(){ volatile int64_t a, b ,c ; __sync_val_compare_and_swap(&a, b, c); return 0;}" HAVE_GCC__SYNC_INT64_CAS )
check_c_source_compiles("int main(){ volatile int a, b ,c ; __sync_val_compare_and_swap(&a, b, c); return 0;}" HAVE_GCC__SYNC_INT32_TAS )
check_c_source_compiles("int main(){ volatile int a, b ,c ; __sync_val_compare_and_swap(&a, b, c); return 0;}" HAVE_GCC__SYNC_INT32_CAS )
check_c_source_compiles("int main(){ volatile char a, b ,c ; __sync_val_compare_and_swap(&a, b, c); return 0;}" HAVE_GCC__SYNC_CHAR_TAS )

check_c_source_compiles("#include <string.h>\nint main(){ char buf[100];  switch (strerror_r(1, buf, sizeof(buf))) { case 0: break; default: break; } return 0;}" STRERROR_R_INT )


check_type_size("long int" SIZE_OF_LONG_INT BUILTIN_TYPES_ONLY)
check_type_size("long long int" SIZE_OF_LONG_LONG_INT BUILTIN_TYPES_ONLY)

if (${SIZE_OF_LONG_INT} EQUAL 8)
	set(HAVE_LONG_INT_64 1)
	set(PG_INT64_TYPE "long")
endif()

if (${SIZE_OF_LONG_LONG_INT} EQUAL 8)
set(PG_INT64_TYPE "long long")
set(HAVE_LONG_LONG_INT_64 1)
endif()

check_include_file(netinet/tcp.h HAVE_NETINET_TCP_H)
check_include_file(net/if.h HAVE_NET_IF_H)
check_include_file(stdbool.h HAVE_STDBOOL_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(sys/un.h HAVE_SYS_UN_H)
check_include_file(ured.h HAVE_UCRED_H)
check_include_file(poll.h HAVE_POLL_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)

check_include_file(sys/ucred.h HAVE_SYS_UCRED_H)
check_include_file(ucred.h HAVE_UCRED_H)
check_include_file(unistd.h HAVE_UNISTD_H)

if (HAVE_UNISTD_H)
	add_definitions(-DHAVE_UNISTD_H)
endif()

if (MSVC)
unset(PG_PRINTF_ATTRIBUTE )
else()
set(PG_PRINTF_ATTRIBUTE gnu_printf)
endif()

add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_WINSOCK_DEPRECATED_NO_WARNINGS -DHAVE_INET_ATON -DFRONTEND)
include_directories(include)

if (WIN32)
	set(LIBPQ_WIN_SRC
		pthread-win32.c
		win32.c
		port/win32stat.c
		port/open.c
		port/win32error.c
		port/win32env.c
		port/win32setlocale.c
		port/pgsleep.c
		port/gettimeofday.c
	)
endif()

if (NOT APPLE)
	set(STRLCPY_SRC port/strlcpy.c)
endif()

file(GLOB_RECURSE libpq_headers
	include
	*.h
)

add_library(libpq STATIC
	${libpq_headers}
	fe-auth-scram.c
	fe-auth.c
	fe-connect.c
	fe-exec.c
	fe-lobj.c
	fe-misc.c
	fe-print.c
	fe-protocol3.c
	fe-secure-common.c
	fe-secure.c
	fe-trace.c
	legacy-pqsignal.c
	libpq-events.c
	pqexpbuffer.c
	port/snprintf.c
	port/thread.c
	port/pgstrcasecmp.c
	port/chklocale.c
	port/pg_strong_random.c
	port/strerror.c
	port/getpeereid.c
	port/noblock.c
	port/inet_net_ntop.c
	port/explicit_bzero.c
	common/cryptohash.c
	common/hmac.c
	common/string.c
	common/md5_common.c
	common/md5.c
	common/encnames.c
	common/wchar.c
	common/base64.c
	common/ip.c
	common/sha1.c
	common/sha2.c
	common/saslprep.c
	common/link-canary.c
	common/unicode_norm.c
	common/scram-common.c

	${STRLCPY_SRC}
	${LIBPQ_WIN_SRC}
)

set_target_properties(libpq PROPERTIES FOLDER "third_party")

set(AC_APPLE_UNIVERSAL_BUILD 0)

configure_file(include/pg_config.h.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/pg_config.h)
configure_file(include/pg_config_ext.h.in ${CMAKE_CURRENT_BINARY_DIR}/pg_config_ext.h)

target_include_directories(libpq PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(libpq PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (WIN32)
	target_compile_definitions(libpq PRIVATE -DWIN32)
endif()

if(MSVC)
	target_include_directories(libpq PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/port)
else()
	target_compile_options(libpq PRIVATE -Wno-ignored-attributes)
endif()

target_compile_definitions(libpq PRIVATE -DPG_VERSION_NUM=140101)
