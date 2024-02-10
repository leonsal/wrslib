cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(wrcproj C)

#
# Dependencies
#
include(cmake/CPM.cmake)
CPMAddPackage(
    NAME cxlib
    GITHUB_REPOSITORY leonsal/cxlib
    GIT_TAG main
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
    GIT_TAG master # OR v1.16
    OPTIONS
        "CIVETWEB_ENABLE_DEBUG_TOOLS OFF"
        "CIVETWEB_ENABLE_CXX OFF"
        "CIVETWEB_BUILD_TESTING OFF"
        "CIVETWEB_ENABLE_SSL OFF"
        "CIVETWEB_ENABLE_SERVER_EXECUTABLE OFF"
        "CIVETWEB_INSTALL_EXECUTABLE OFF"
        "CIVETWEB_ENABLE_WEBSOCKETS ON"
        "CIVETWEB_ENABLE_ASAN OFF"
        "CIVETWEB_ENABLE_KEEPALIVE ON"
)

set(SOURCES
    include/wrc.h
    src/wrc.c
)

add_library(wrc ${SOURCES})
target_include_directories(wrc
    PUBLIC ${PROJECT_SOURCE_DIR}/include
    PUBLIC ${cxlib_SOURCE_DIR}/include
    PUBLIC ${civetweb_SOURCE_DIR}/include
)
target_link_libraries(wrc cxlib civetweb-c-library zip m)

