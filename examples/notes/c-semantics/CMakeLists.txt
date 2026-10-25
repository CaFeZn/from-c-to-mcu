cmake_minimum_required(VERSION 3.16)
project(c_semantics_notes LANGUAGES C)

enable_testing()

set(note_names
    volatile-optimization
    const-restrict
    strict-aliasing
    evaluation-order
    dynamic-memory
    strtok-reentrancy
    advanced-macros
)

foreach(note IN LISTS note_names)
    string(REPLACE "-" "_" target "${note}")
    add_executable(${target} ${note}.c)
    set_target_properties(${target} PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror -pedantic-errors -UNDEBUG
        )
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /UNDEBUG)
    endif()
    # The lesson deliberately demonstrates the ISO C strtok API.
    if(WIN32 AND note STREQUAL "strtok-reentrancy")
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endforeach()
