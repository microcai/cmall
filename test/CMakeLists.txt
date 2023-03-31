
add_executable(test_http_client httpc/test_http_client.cpp
../cmall/src/default_cert.cpp)
# target_precompile_headers(test_http_client PUBLIC
#   "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/stdafx.hpp>"
# )


target_include_directories(test_http_client PRIVATE
	${CMAKE_SOURCE_DIR}/cmall/include
)


add_executable(test_rsa_sign test_rsa.cpp)


target_link_libraries(test_rsa_sign ${OPENSSL_LIBRARIES})

add_executable(test_wx test_wx.cpp)

target_link_libraries(test_wx cmall_service)

add_executable(test_decimal test_decimal.cpp)
target_link_libraries(test_decimal Boost::system)