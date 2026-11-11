cmake_minimum_required(VERSION 3.20)
project(cpp_particles CXX)

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

# Pure simulation: no SDL, so it can be tested without a window.
add_library(particles_core STATIC src/particles.cpp)
target_include_directories(particles_core PUBLIC src)
target_compile_options(particles_core PRIVATE ${PROJECT_WARNINGS})

add_executable(cpp_particles src/main.cpp)
target_link_libraries(cpp_particles PRIVATE particles_core SDL3::SDL3)
target_compile_options(cpp_particles PRIVATE ${PROJECT_WARNINGS})

enable_testing()
add_executable(particles_test tests/particles_test.cpp)
target_link_libraries(particles_test PRIVATE particles_core)
target_compile_options(particles_test PRIVATE ${PROJECT_WARNINGS})
add_test(NAME particles COMMAND particles_test)
