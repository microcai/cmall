add_executable(test_http_client httpc/test_http_client.cpp
../cmall/src/default_cert.cpp)
# target_precompile_headers(test_http_client PUBLIC
#   "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/stdafx.hpp>"
# )
target_include_directories(test_http_client PRIVATE
	${CMAKE_SOURCE_DIR}/cmall/include
)
