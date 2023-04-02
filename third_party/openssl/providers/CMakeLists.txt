
file(GLOB SRCS
    baseprov.c
    nullprov.c
    defltprov.c
    prov_running.c

    common/*.c
    common/der/*.c
    implementations/asymciphers/*.c
    implementations/ciphers/*.c
    implementations/digests/*.c
    implementations/exchange/*.c

    implementations/kdfs/*.c
    implementations/kem/*.c
    implementations/keymgmt/*.c
    implementations/macs/*.c
    implementations/rands/*.c
    implementations/rands/seeding/rand_unix.c implementations/rands/seeding/rand_win.c implementations/rands/seeding/rand_tsc.c implementations/rands/seeding/rand_cpu_x86.c
    implementations/encode_decode/*.c
    implementations/signature/*.c
    implementations/storemgmt/*.c

    fips/fipsprovc.c
)

list(FILTER SRCS EXCLUDE REGEX "securitycheck_default.c")

list(FILTER SRCS EXCLUDE REGEX "md2_*")

list(FILTER SRCS EXCLUDE REGEX "blake2_mac_impl*")

add_library(openssl_providers OBJECT ${SRCS})

target_include_directories(openssl_providers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/implementations/include
)

target_link_libraries(openssl_providers openssl_common)

target_compile_definitions(openssl_providers PRIVATE
    -DOPENSSL_RAND_SEED_OS=Linux
    -DSTATIC_LEGACY
)