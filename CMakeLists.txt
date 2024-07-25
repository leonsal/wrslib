cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(wrslib C)


# Use CMake Package Manager for external dependencies
# https://github.com/cpm-cmake/CPM.cmake
include(cmake/CPM.cmake)

CPMAddPackage(
    NAME cxlib
    GITHUB_REPOSITORY leonsal/cxlib
    #GIT_TAG main
    GIT_TAG cxerror
)

CPMAddPackage(
    NAME libzip
    GITHUB_REPOSITORY nih-at/libzip
    GIT_TAG v1.10.1
    OPTIONS
        "ENABLE_COMMONCRYPTO OFF"
        "ENABLE_GNUTLS OFF"
        "ENABLE_MBEDTLS OFF"
        "ENABLE_OPENSSL OFF"
        "ENABLE_WINDOWS_CRYPTO OFF"
        "ENABLE_BZIP2 OFF"
        "ENABLE_LZMA OFF"
        "ENABLE_ZSTD OFF"
        "BUILD_SHARED_LIBS OFF"
        "BUILD_TOOLS OFF"
        "BUILD_REGRESS OFF"
        "BUILD_EXAMPLES OFF"
        "BUILD_DOC OFF"
)

CPMAddPackage(
    NAME civetweb
    GITHUB_REPOSITORY civetweb/civetweb
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

#
# Builds library with civetweb sources
#
add_library(civetweb 
    ${civetweb_SOURCE_DIR}/src/civetweb.c
    ${civetweb_SOURCE_DIR}/src/md5.inl
    ${civetweb_SOURCE_DIR}/src/sha1.inl
    ${civetweb_SOURCE_DIR}/src/handle_form.inl
    ${civetweb_SOURCE_DIR}/src/response.inl
    ${civetweb_SOURCE_DIR}/src/sort.inl
    ${civetweb_SOURCE_DIR}/src/match.inl
    ${civetweb_SOURCE_DIR}/src/timer.inl
)

target_include_directories(civetweb
    PRIVATE ${civetweb_SOURCE_DIR}/include
)

target_compile_definitions(civetweb PRIVATE
    NO_SSL
    USE_WEBSOCKET
    REENTRANT_TIME
    #STOP_FLAG_NEEDS_LOCK
)

set(SOURCES
    include/wrs.h
    src/server.c
    src/rpc.c
    src/rpc_codec.h
    src/rpc_codec.c
)

add_library(wrs ${SOURCES})
target_include_directories(wrs
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PUBLIC ${cxlib_SOURCE_DIR}/include
    PUBLIC ${civetweb_SOURCE_DIR}/include
)
target_link_libraries(wrs
    cxlib
    civetweb
    zip
    m
)

