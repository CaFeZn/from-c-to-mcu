#!/usr/bin/env bash
set -eu
out=artifacts/course-completion/c-functions/core-build
src=examples/notes/c-functions
mkdir -p "$out"
for name in pointer_demo patterns_demo; do
    gcc -std=c99 -O0 -Wall -Wextra -Werror -pedantic "$src/$name.c" -o "$out/$name.exe"
    "$out/$name.exe"
done
gcc -std=c99 -Wall -Wextra -Werror -pedantic -c "$src/cpp_c_api.c" -o "$out/cpp_c_api.o"
g++ -std=c++11 -Wall -Wextra -Werror -pedantic "$src/embedded_cpp_demo.cpp" "$out/cpp_c_api.o" -o "$out/embedded_cpp_demo.exe"
"$out/embedded_cpp_demo.exe"
g++ -std=c++11 -fno-exceptions -fno-rtti -Os -Wall -Wextra -Werror -pedantic "$src/embedded_cpp_demo.cpp" "$out/cpp_c_api.o" -o "$out/embedded_cpp_noeh.exe"
"$out/embedded_cpp_noeh.exe"
