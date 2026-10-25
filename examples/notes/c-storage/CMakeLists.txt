cmake_minimum_required(VERSION 3.16)
project(c_storage_notes LANGUAGES C)
enable_testing()

set(storage_examples
    variables-memory
    array-pointer-arithmetic
    struct-alignment
    buffer-bounds
    pointer-lifetime
    packed-bitfields-unions
    byte-order
)

foreach(example IN LISTS storage_examples)
    add_executable(${example} ${example}.c)
    set_target_properties(${example} PROPERTIES
        C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    if(MINGW)
        target_compile_definitions(${example} PRIVATE __USE_MINGW_ANSI_STDIO=1)
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${example} PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
    endif()
    if(example STREQUAL "buffer-bounds" AND CMAKE_C_COMPILER_ID STREQUAL "GNU"
       AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 7)
        # This experiment intentionally verifies snprintf truncation. Keep its warning.
        target_compile_options(${example} PRIVATE -Wno-error=format-truncation)
    endif()
    add_test(NAME ${example} COMMAND ${example})
endforeach()
