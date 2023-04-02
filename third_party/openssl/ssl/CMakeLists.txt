
file(GLOB SRCS *.c statem/*.c record/*.c)

include_directories(../crypto/)

list(FILTER SRCS EXCLUDE REGEX "ktls.c")

if (SHARED_OPENSSL)
add_library(ssl SHARED ${SRCS})
else()
add_library(ssl STATIC ${SRCS})
endif()

target_link_libraries(ssl crypto)

set_target_properties(ssl PROPERTIES FOLDER "third_party/openssl")

target_include_directories(ssl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/)

target_compile_definitions(ssl PUBLIC -DOPENSSL_NO_ENGINE -DOPENSSL_NO_KTLS)

target_compile_definitions(ssl INTERFACE -DOPENSSL_NO_ENGINE)