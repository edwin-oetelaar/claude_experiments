cmake_minimum_required(VERSION 3.20)
project(c_echo_http C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
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

# Strict C17 hides POSIX functions (sigaction, ...); ask for them explicitly.
add_compile_definitions(_POSIX_C_SOURCE=200809L)

# Parsing and responses: no sockets, so it can be tested without a network.
add_library(echo_core STATIC src/http.c src/app.c)
target_include_directories(echo_core PUBLIC src)
target_compile_options(echo_core PRIVATE ${PROJECT_WARNINGS})

add_executable(c_echo_http src/main.c)
target_link_libraries(c_echo_http PRIVATE echo_core)
target_compile_options(c_echo_http PRIVATE ${PROJECT_WARNINGS})

enable_testing()
add_executable(http_test tests/http_test.c)
target_link_libraries(http_test PRIVATE echo_core)
target_compile_options(http_test PRIVATE ${PROJECT_WARNINGS})
add_test(NAME http COMMAND http_test)
