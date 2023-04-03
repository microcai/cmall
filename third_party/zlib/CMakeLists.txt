cmake_minimum_required(VERSION 3.0)
project(zlib)
find_program(ML64 ml64.exe)

if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86|AMD64")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(AMD64 1)
else()
    set(ASM686 1)
endif()
else()
    set (ASM686 0)
    set (AMD64 0)
endif()

if(CMAKE_COMPILER_IS_GNUCC)
	if(ASM686)
		set(ZLIB_ASMS contrib/asm686/match.S)
	elseif (AMD64)
		set(ZLIB_ASMS contrib/amd64/amd64-match.S)
	endif ()

	if(ZLIB_ASMS)
		add_definitions(-DASMV)
		set_source_files_properties(${ZLIB_ASMS} PROPERTIES LANGUAGE C COMPILE_FLAGS -DNO_UNDERLINE)
	endif()
endif()

if(MSVC AND ML64)
    enable_language(ASM_MASM)
    message(STATUS "found masm, enable ASM accel code path")
    if(ASM686)
        set(ZLIB_ASMS
            contrib/masmx86/inffas32.asm
            contrib/masmx86/match686.asm
        )
    elseif (AMD64)
        set(ZLIB_ASMS
            contrib/masmx64/gvmat64.asm
            contrib/masmx64/inffasx64.asm
            contrib/masmx64/inffas8664.c
        )
        set_source_files_properties(
            contrib/masmx64/gvmat64.asm 
            contrib/masmx64/inffasx64.asm
            PROPERTIES COMPILE_FLAGS "/c /Zi"
        )

    endif()

    if(ZLIB_ASMS)
        add_definitions(-DASMV -DASMINF)
    endif()
endif()


file(GLOB ZLIB_SRC *.c *.h)

add_library(zlib STATIC ${ZLIB_SRC} ${ZLIB_ASMS})

target_include_directories(zlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(zlib PROPERTIES FOLDER "third_party")
set_target_properties(zlib PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(zlib PROPERTIES OUTPUT_NAME z PREFIX "lib" CLEAN_DIRECT_OUTPUT 1)

