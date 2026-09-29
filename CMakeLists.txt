cmake_minimum_required(VERSION 3.10)

project(elecard_yuv_overlay LANGUAGES CXX)

find_package(Threads REQUIRED)

add_executable(yuv-overlay
    src/main.cpp
    src/bmp.cpp
    src/yuv.cpp
    src/video.cpp
)

set_target_properties(yuv-overlay PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_link_libraries(yuv-overlay PRIVATE Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(yuv-overlay PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_executable(yuv-overlay-tests
    tests/tests.cpp
    src/bmp.cpp
    src/yuv.cpp
    src/video.cpp
)
target_include_directories(yuv-overlay-tests PRIVATE src)
set_target_properties(yuv-overlay-tests PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_link_libraries(yuv-overlay-tests PRIVATE Threads::Threads)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(yuv-overlay-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME yuv-overlay-tests COMMAND yuv-overlay-tests)