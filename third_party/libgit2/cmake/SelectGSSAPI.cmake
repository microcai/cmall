include(SanitizeBool)

# We try to find any packages our backends might use
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
	include(FindGSSFramework)
endif()

if(USE_GSSAPI)
	find_package(GSSAPI)
	# Auto-select GSS backend
	sanitizebool(USE_GSSAPI)
	if(USE_GSSAPI STREQUAL ON)
		if(GSSFRAMEWORK_FOUND)
			set(USE_GSSAPI "GSS.framework")
		elseif(GSSAPI_FOUND)
			set(USE_GSSAPI "gssapi")
		else()
			message(FATAL_ERROR "Unable to autodetect a usable GSS backend."
				"Please pass the backend name explicitly (-DUSE_GSS=backend)")
		endif()
	endif()

	# Check that we can find what's required for the selected backend
	if(USE_GSSAPI STREQUAL "GSS.framework")
		if(NOT GSSFRAMEWORK_FOUND)
			message(FATAL_ERROR "Asked for GSS.framework backend, but it wasn't found")
		endif()

		list(APPEND LIBGIT2_SYSTEM_LIBS ${GSSFRAMEWORK_LIBRARIES})

		set(GIT_GSSFRAMEWORK 1)
		add_feature_info(SPNEGO GIT_GSSFRAMEWORK "SPNEGO authentication support (${USE_GSSAPI})")
	elseif(USE_GSSAPI STREQUAL "gssapi")
		if(NOT GSSAPI_FOUND)
			message(FATAL_ERROR "Asked for gssapi GSS backend, but it wasn't found")
		endif()

		list(APPEND LIBGIT2_SYSTEM_LIBS ${GSSAPI_LIBRARIES})

		set(GIT_GSSAPI 1)
		add_feature_info(SPNEGO GIT_GSSAPI "SPNEGO authentication support (${USE_GSSAPI})")
	else()
		message(FATAL_ERROR "Asked for backend ${USE_GSSAPI} but it wasn't found")
	endif()
else()
	set(GIT_GSSAPI 0)
	add_feature_info(SPNEGO NO "SPNEGO authentication support")
endif()
