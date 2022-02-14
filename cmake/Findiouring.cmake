# - Find ioiouring
# Find the native ioiouring includes and library
#
#  iouring_INCLUDE_DIR - where to find Tcmalloc.h, etc.
#  iouring_LIBRARIES   - List of libraries when using Tcmalloc.
#  iouring_FOUND       - True if Tcmalloc found.


find_path(iouring_INCLUDE_DIR liburing.h)

find_library(iouring_LIBRARY NAMES uring)

if(iouring_INCLUDE_DIR AND iouring_LIBRARY)
	set(iouring_FOUND TRUE)
	set(iouring_LIBRARIES ${iouring_LIBRARY})
else()
	set(iouring_FOUND FALSE)
	set(iouring_LIBRARIES )
endif()

if(iouring_FOUND)
	message(STATUS "Found iouring: ${iouring_LIBRARY}")
else()
	message(STATUS "iouring not found")
	if (iouring_FIND_REQUIRED)
		message(FATAL_ERROR "Could NOT find iouring library")
	endif()
endif()

mark_as_advanced(
	iouring_INCLUDE_DIR
	iouring_LIBRARY
	iouring_LIBRARIES
)