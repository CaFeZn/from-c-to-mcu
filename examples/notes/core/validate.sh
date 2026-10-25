#!/usr/bin/env bash
set -euo pipefail
# Run from repository root; output stays inside the core audit scope.
OUT=artifacts/course-completion/core
SRC=examples/notes/core
ARM=/d/ST/STM32CubeCLT_1.16.0/GNU-tools-for-STM32/bin/arm-none-eabi-gcc.exe
printf '%s\n' '=== Host compiler ==='
gcc --version
for opt in 0 2; do
  gcc -std=c99 -O"$opt" -Wall -Wextra -Wpedantic -Werror "$SRC/test.c" -o "$OUT/models-o$opt.exe"
  "$OUT/models-o$opt.exe"
done
for name in read-modify-write vector-table interrupt-storms exclusive-access; do
  gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror "$SRC/snippets/$name.c" -o "$OUT/$name.exe"
  "$OUT/$name.exe"
done
printf '%s\n' '=== Arm compiler ==='
"$ARM" --version
"$ARM" -std=c99 -mcpu=cortex-m4 -mthumb -O2 -Wall -Wextra -Werror -Iartifacts/mcu-principles-reference -I"$OUT/reference" -c "$SRC/board_snippets.c" -o "$OUT/board_snippets.o"
"$ARM" -std=c99 -mcpu=cortex-m4 -mthumb -O2 -Wall -Wextra -Werror -Iartifacts/mcu-principles-reference -I"$OUT/reference" -S "$SRC/board_snippets.c" -o "$OUT/board_snippets.s"
printf '%s\n' '=== Isolated CMake/CTest ==='
cmake -S "$SRC" -B "$OUT/host-build" -G Ninja -DCMAKE_C_COMPILER=C:/MinGW/bin/gcc.exe -DCMAKE_BUILD_TYPE=Release
cmake --build "$OUT/host-build"
ctest --test-dir "$OUT/host-build" --output-on-failure
