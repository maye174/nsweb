cmake_minimum_required(VERSION 3.15)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/extra.cmake)

# toolchain
message("building for SWITCH")
message("Build Type: ${CMAKE_BUILD_TYPE}")
message("DEVKITPRO: $ENV{DEVKITPRO}")
set(CMAKE_USE_SYSTEM_ENVIRONMENT_PATH OFF)
file(TO_CMAKE_PATH "$ENV{DEVKITPRO}" DEVKITPRO)
if (NOT IS_DIRECTORY ${DEVKITPRO})
    message(FATAL_ERROR "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif ()
set(DEVKITPRO $ENV{DEVKITPRO} CACHE BOOL "DEVKITPRO")
include(${DEVKITPRO}/cmake/Switch.cmake REQUIRED)
set(__SWITCH__ ON)
set(CMAKE_C_FLAGS "-I${DEVKITPRO}/libnx/include -I${DEVKITPRO}/portlibs/switch/include")
set(CMAKE_CXX_FLAGS "-I${DEVKITPRO}/libnx/include -I${DEVKITPRO}/portlibs/switch/include")
message(STATUS "PKG_CONFIG_EXECUTABLE = ${PKG_CONFIG_EXECUTABLE}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS = ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_C_COMPILER = ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER = ${CMAKE_CXX_COMPILER}")
message(STATUS "NX_NACP" = ${NX_NACPTOOL_EXE})
message(STATUS "NX_ELF2NRO" = ${NX_ELF2NRO_EXE})


# project info
project(nsweb)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
add_definitions(-D__SWITCH__)

link_directories(${DEVKITPRO}/libnx/lib ${DEVKITPRO}/portlibs/switch/lib)
# setting src and include
file(GLOB_RECURSE MAIN_SRC src/*.cpp)
#file(GLOB_RECURSE MAIN_SRC src/*.c)

#find_library(NX_LIB nx PATHS ${DEVKITPRO}/portlibs/switch/lib)
#find_library(M_LIB m)
#find_library(NSP_LIB nsp)
list(APPEND PLATFORM_LIBS
    # needed by borealis
    EGL glapi drm_nouveau
    # needed by mpv
    mpv swscale swresample avformat avfilter avcodec avutil ass freetype fribidi png bz2 z SDL2
    mbedx509 mbedtls mbedcrypto mbedx509 mbedtls
    # needed by cpr and api/danmaku_live
    curl z
    # base lib
    nx m
    # added switch-wslay library
    wslay
)

set(PLATFORM_OPTION -DBUILTIN_NSP)

add_executable(${PROJECT_NAME} ${MAIN_SRC})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${PLATFORM_LIBS}
)

# building release file
add_custom_target(${PROJECT_NAME}.nro
            DEPENDS ${PROJECT_NAME}
            COMMAND ${NX_NACPTOOL_EXE} --create "${PROJECT_NAME}"
            ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
            COMMAND ${NX_ELF2NRO_EXE} 
            --nacp=${PROJECT_NAME}.nacp )