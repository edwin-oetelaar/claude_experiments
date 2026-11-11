cmake_minimum_required(VERSION 3.20)
project(cpp_synth CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# An empty build type means no optimisation at all; default to Release.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

set(PROJECT_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)

find_package(SDL3 REQUIRED CONFIG)

# Pure sound generation and file writing: no SDL, so it can be tested without a sound card.
add_library(synth_core STATIC src/synth.cpp src/wav.cpp)
target_include_directories(synth_core PUBLIC src)
target_compile_options(synth_core PRIVATE ${PROJECT_WARNINGS})

add_executable(cpp_synth src/main.cpp)
target_link_libraries(cpp_synth PRIVATE synth_core SDL3::SDL3)
target_compile_options(cpp_synth PRIVATE ${PROJECT_WARNINGS})

enable_testing()
add_executable(synth_test tests/synth_test.cpp)
target_link_libraries(synth_test PRIVATE synth_core)
target_compile_options(synth_test PRIVATE ${PROJECT_WARNINGS})
add_test(NAME synth COMMAND synth_test)
