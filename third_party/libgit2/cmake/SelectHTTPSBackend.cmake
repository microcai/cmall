include(SanitizeBool)

# We try to find any packages our backends might use
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
	find_package(Security)
	find_package(CoreFoundation)
endif()

set(GIT_HTTPS 0)
add_feature_info(HTTPS NO "")
