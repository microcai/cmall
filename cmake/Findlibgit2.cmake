# - Find Tcmalloc
# Find the native Tcmalloc includes and library
#
#  Tcmalloc_INCLUDE_DIR - where to find Tcmalloc.h, etc.
#  Tcmalloc_LIBRARIES   - List of libraries when using Tcmalloc.
#  Tcmalloc_FOUND       - True if Tcmalloc found.

find_path(LIBGIT2_INCLUDE_DIR git2.h)

find_library(LIBGIT2_LIBRARY NAMES git2)

if (LIBGIT2_INCLUDE_DIR AND LIBGIT2_LIBRARY)
  set(LIBGIT2_FOUND TRUE)
  set(LIBGIT2_LIBRARIES ${LIBGIT2_LIBRARY} )
else ()
  set(LIBGIT2_FOUND FALSE)
  set( LIBGIT2_LIBRARIES )
endif ()

if (LIBGIT2_FOUND)
  message(STATUS "Found libLIBGIT2: ${LIBGIT2_LIBRARY}")
else ()
  message(STATUS "Not Found libLIBGIT2")
  if (LIBGIT2_FIND_REQUIRED)
	  message(STATUS "Looked for libLIBGIT2 libraries named ${LIBGIT2_LIBRARY}.")
	  message(FATAL_ERROR "Could NOT find IOUring library")
  endif ()
endif ()

mark_as_advanced(
  LIBGIT2_LIBRARY
  LIBGIT2_INCLUDE_DIR
)

