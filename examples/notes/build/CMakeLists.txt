cmake_minimum_required(VERSION 3.20)
project(build_notes C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(REPORT_MODE "Enable the report marker" ON)
enable_testing()

add_library(clamp STATIC src/clamp.c)
target_include_directories(clamp PUBLIC include)
add_executable(clamp_demo src/main.c)
target_link_libraries(clamp_demo PRIVATE clamp)
if(REPORT_MODE)
  target_compile_definitions(clamp_demo PRIVATE REPORT_MODE=1)
else()
  target_compile_definitions(clamp_demo PRIVATE REPORT_MODE=0)
endif()
add_executable(toolchain_probe src/probe.c)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated.h"
  COMMAND "${CMAKE_COMMAND}"
          "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/schema.txt"
          "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/generated.h"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/generate.cmake"
  DEPENDS schema.txt generate.cmake
  VERBATIM)
add_executable(generated_demo src/generated.c
               "${CMAKE_CURRENT_BINARY_DIR}/generated.h")
target_include_directories(generated_demo PRIVATE
                           "${CMAKE_CURRENT_BINARY_DIR}")
foreach(t clamp clamp_demo toolchain_probe generated_demo)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endforeach()
add_test(NAME clamp COMMAND clamp_demo)
add_test(NAME toolchain COMMAND toolchain_probe)
add_test(NAME generated COMMAND generated_demo)
