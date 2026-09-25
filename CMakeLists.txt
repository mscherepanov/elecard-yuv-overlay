cmake_minimum_required(VERSION 3.10)

project(elecard_yuv_overlay LANGUAGES CXX)

add_executable(yuv-overlay
    src/main.cpp
    src/bmp.cpp
)

set_target_properties(yuv-overlay PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(yuv-overlay PRIVATE -Wall -Wextra -Wpedantic)
endif()