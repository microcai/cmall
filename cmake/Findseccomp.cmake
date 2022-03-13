# - Find Tcmalloc
# Find the native Tcmalloc includes and library
#
#  Tcmalloc_INCLUDE_DIR - where to find Tcmalloc.h, etc.
#  Tcmalloc_LIBRARIES   - List of libraries when using Tcmalloc.
#  Tcmalloc_FOUND       - True if Tcmalloc found.

find_path(SECCOMP_INCLUDE_DIR seccomp.h)

find_library(SECCOMP_LIBRARY NAMES seccomp)

if (SECCOMP_INCLUDE_DIR AND SECCOMP_LIBRARY)
  set(SECCOMP_FOUND TRUE)
  set(SECCOMP_LIBRARIES ${SECCOMP_LIBRARY} )
else ()
  set(SECCOMP_FOUND FALSE)
  set( SECCOMP_LIBRARIES )
endif ()

if (SECCOMP_FOUND)
  message(STATUS "Found libseccomp: ${SECCOMP_LIBRARY}")
else ()
  message(STATUS "Not Found libseccomp")
  if (SECCOMP_FIND_REQUIRED)
	  message(STATUS "Looked for libseccomp libraries named ${SECCOMP_LIBRARY}.")
	  message(FATAL_ERROR "Could NOT find IOUring library")
  endif ()
endif ()

mark_as_advanced(
  SECCOMP_LIBRARY
  SECCOMP_INCLUDE_DIR
)

