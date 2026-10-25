cmake_minimum_required(VERSION 3.16)
project(serial_crc_examples LANGUAGES C)

add_library(serial_crc STATIC crc.c)
target_include_directories(serial_crc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(serial_crc_test test_crc.c)
target_link_libraries(serial_crc_test PRIVATE serial_crc)

foreach(target serial_crc serial_crc_test)
    set_target_properties(${target} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wconversion -Wshadow -Wpedantic -Werror)
    endif()
endforeach()

enable_testing()
add_test(NAME serial_crc_vectors COMMAND serial_crc_test)
