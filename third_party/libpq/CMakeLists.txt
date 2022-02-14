add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_WINSOCK_DEPRECATED_NO_WARNINGS -DHAVE_INET_ATON -DFRONTEND)
include_directories(include)
add_library(libpq STATIC
	src/win32.c
	src/fe_memutils.c
	src/libpq-events.c
	src/strlcpy.c
	src/fe-exec.c
	src/wchar.c
	src/pqexpbuffer.c
	src/md5.c
	src/open.c
	src/fe-protocol2.c
	src/win32error.c
	src/dirmod.c
	src/pgsleep.c
	src/fe-lobj.c
	src/pthread-win32.c
	src/noblock.c
	src/pgstrcasecmp.c
	src/encnames.c
	src/fe-print.c
	src/pqsignal.c
	src/getaddrinfo.c
	src/fe-protocol3.c
	src/fe-connect.c
	src/thread.c
	src/fe-secure.c
	src/inet_net_ntop.c
	src/dirent.c
	src/ip.c
	src/snprintf.c
	src/inet_aton.c
	src/fe-auth.c
	src/fe-misc.c
	src/crypt.c
	src/chklocale.c
	src/win32setlocale.c
)

set_target_properties(libpq PROPERTIES FOLDER "third_party")
