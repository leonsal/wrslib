cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(wrctests C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

#
# Tests dependencies
#
include(cmake/CPM.cmake)

CPMAddPackage(
    NAME linenoise
    GITHUB_REPOSITORY antirez/linenoise
    GIT_TAG master
)

CPMAddPackage(
    NAME incbin
    GITHUB_REPOSITORY graphitemaster/incbin
    GIT_TAG main
)

CPMAddPackage(
    NAME argparse
    GITHUB_REPOSITORY cofyc/argparse
    GIT_TAG master
    OPTIONS "ARGPARSE_SHARED OFF"
)

set(SOURCES
    src/main.c
    src/staticfs.c
    src/cli.h
    src/cli.c
    ${linenoise_SOURCE_DIR}/linenoise.c
)

add_subdirectory(.. wrc)
add_executable(tests ${SOURCES})
target_include_directories(tests
    PUBLIC ${incbin_SOURCE_DIR}
    PUBLIC ${linenoise_SOURCE_DIR}
)
set_property(TARGET tests PROPERTY C_STANDARD  11)
target_compile_options(tests PRIVATE -Wall -Wno-unused-function)
target_link_libraries(tests wrs argparse)
