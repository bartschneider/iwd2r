# iwd2r. Distributed under LICENSE.md.
cmake_minimum_required(VERSION 3.28)

project(iwd2r VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

if(NOT MSVC)
    add_compile_options(-fsigned-char)
endif()

function(iwd2r_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
# libXtst is only used for synthetic input; do not require its dev package.
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.18
    GIT_SHALLOW TRUE)
FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.25
    SOURCE_SUBDIR iwd2r-header-only
    GIT_SHALLOW TRUE)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL3 miniaudio Catch2)

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE "${miniaudio_SOURCE_DIR}")

add_subdirectory(third_party/zlib)

enable_testing()
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

add_subdirectory(compat)
add_subdirectory(platform)

# Decompiled engine. Builds with warnings allowed (no iwd2r_strict_warnings).
include(cmake/engine_sources.cmake)
add_library(iwd2r_engine OBJECT ${IWD2R_ENGINE_SOURCES})
target_include_directories(iwd2r_engine PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/compat"
    "${PROJECT_SOURCE_DIR}"
    ${ZLIB_INCLUDE_DIRS})
target_link_libraries(iwd2r_engine PRIVATE iwd2r_compat iwd2r_platform ${ZLIB_LIBRARIES})

# The game. A console program for now, so the smoke run's stdout is visible on
# Windows too.
add_executable(iwd2r platform/entry.cpp $<TARGET_OBJECTS:iwd2r_engine>)
target_include_directories(iwd2r PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(iwd2r PRIVATE iwd2r_platform iwd2r_compat ${ZLIB_LIBRARIES})
iwd2r_strict_warnings(iwd2r)
add_subdirectory(tests)
