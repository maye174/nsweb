cmake_minimum_required(VERSION 3.15.0)
project(nsweb LANGUAGES CXX C)

option(BUILD_TESTS "Build nsweb test cases" OFF)
set(WSLAY_STATIC ON)
set(WSLAY_CONFIGURE_INSTALL OFF)
add_subdirectory(wslay)

# target
add_library(nsweb STATIC "")
set_target_properties(nsweb PROPERTIES OUTPUT_NAME "nsweb")
set_target_properties(nsweb PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
target_include_directories(nsweb
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(nsweb PRIVATE cxx_std_11)
target_link_libraries(nsweb PRIVATE
    curl
    wslay
    mbedtls
    mbedcrypto
    mbedx509
)

target_sources(nsweb PRIVATE
    src/nsweb.cpp
    src/socket_fun.cpp
    src/tls_fun.cpp
    src/url_fun.cpp
)

if(BUILD_TESTS)
    # target
    add_executable(nsweb_test "")
    set_target_properties(nsweb_test PROPERTIES OUTPUT_NAME "nsweb_test")
    set_target_properties(nsweb_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_dependencies(nsweb_test nsweb)
    target_include_directories(nsweb_test PRIVATE
        src
    )
    target_compile_features(nsweb_test PRIVATE cxx_std_11)
    target_link_libraries(nsweb_test PRIVATE
        curl
        wslay
        mbedtls
        mbedcrypto
        mbedx509
        nsweb
    )

    target_sources(nsweb_test PRIVATE
        test/main.cpp
    )
endif()
