cmake_minimum_required(VERSION 3.16)
project(can_note_checks LANGUAGES C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
enable_testing()

set(CAN_CHECKS
  can-model-tests
  can-bit-timing-example
  can-error-states-example
  bxcan-filters-example
  can-fd-canopen-example
  can-extended-remote-example
  can-load-priority-example
)
foreach(check IN LISTS CAN_CHECKS)
  if(check STREQUAL "can-model-tests")
    add_executable(${check} test.c)
  else()
    add_executable(${check} ${check}.c)
  endif()
  if(MSVC)
    target_compile_options(${check} PRIVATE /W4 /WX /UNDEBUG)
  else()
    target_compile_options(${check} PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
  endif()
  add_test(NAME ${check} COMMAND ${check})
endforeach()
