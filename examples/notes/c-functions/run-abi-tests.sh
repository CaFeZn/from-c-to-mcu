#!/usr/bin/env bash
# Run from the repository root with Bash. No CMake or global build required.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$root"
src=examples/notes/c-functions
out=artifacts/course-completion/c-functions/abi-build
mkdir -p "$out"
host="${HOST_CC:-$(command -v gcc || true)}"
arm="${ARM_CC:-$(command -v arm-none-eabi-gcc || true)}"
objdump="${ARM_OBJDUMP:-$(command -v arm-none-eabi-objdump || true)}"
readelf="${ARM_READELF:-$(command -v arm-none-eabi-readelf || true)}"
for tool in "$host" "$arm" "$objdump" "$readelf"; do
    if [[ -z "$tool" ]]; then
        printf '%s\n' 'Required tool missing; set HOST_CC/ARM_CC/ARM_OBJDUMP/ARM_READELF.' >&2
        exit 2
    fi
done
printf 'HOST_CC=%s\nARM_CC=%s\n' "$host" "$arm"
"$host" --version
"$arm" --version
warnings=(-std=c99 -Wall -Wextra -Werror -pedantic)
stack_expected=$'stack_step(0) = 10\nstack_step(1) = 21\nstack_step(3) = 46\nstack_leaf(41) = 42'
abi_expected=$'sum5=15 keep=22 tail=10 after=11\ntail_sum(3)=6 float=3.75'
for opt in 0 2; do
    "$host" "${warnings[@]}" -O"$opt" "$src/stack_demo.c" -o "$out/stack-host-O$opt.exe"
    actual="$("$out/stack-host-O$opt.exe")"
    actual="${actual//$'\r'/}" # Normalize Windows text-mode CRLF.
    printf '[host O%s stack]\n%s\n' "$opt" "$actual"
    [[ "$actual" == "$stack_expected" ]] || { printf '%s\n' 'Unexpected stack output' >&2; exit 1; }
    "$host" "${warnings[@]}" -O"$opt" "$src/abi_demo.c" "$src/abi_targets.c" -o "$out/abi-host-O$opt.exe"
    actual="$("$out/abi-host-O$opt.exe")"
    actual="${actual//$'\r'/}"
    printf '[host O%s ABI]\n%s\n' "$opt" "$actual"
    [[ "$actual" == "$abi_expected" ]] || { printf '%s\n' 'Unexpected ABI output' >&2; exit 1; }
done
arm_common=("${warnings[@]}" -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -fno-inline -fstack-usage)
compile_arm() {
    local name="$1" source="$2" macro="$3" opt="$4" mode="$5"
    shift 5
    "$arm" "${arm_common[@]}" -O"$opt" -mfloat-abi="$mode" -D"$macro" "$@" -c "$src/$source" -o "$out/$name.o"
    "$arm" "${arm_common[@]}" -O"$opt" -mfloat-abi="$mode" -D"$macro" "$@" -S "$src/$source" -o "$out/$name.s"
    "$objdump" -dr "$out/$name.o" > "$out/$name.dis.txt"
    "$readelf" -A "$out/$name.o" > "$out/$name.attributes.txt"
    printf '[ARM compiled] %s\n' "$name"
}
for opt in 0 2; do
    compile_arm "stack-O$opt" stack_demo.c STACK_NO_MAIN "$opt" soft -fno-optimize-sibling-calls
    compile_arm "abi-soft-O$opt" abi_demo.c ABI_NO_MAIN "$opt" soft
    compile_arm "abi-targets-soft-O$opt" abi_targets.c ABI_NO_MAIN "$opt" soft
done
compile_arm abi-nosibling-O2 abi_demo.c ABI_NO_MAIN 2 soft -fno-optimize-sibling-calls
for mode in softfp hard; do
    compile_arm "abi-$mode-O2" abi_demo.c ABI_NO_MAIN 2 "$mode"
    compile_arm "abi-targets-$mode-O2" abi_targets.c ABI_NO_MAIN 2 "$mode"
done
printf '%s\n' 'PASS: four host runs; eleven Cortex-M4 object/assembly builds.'
printf '%s\n' 'ARM files were compiled/disassembled only, not linked into firmware or run on hardware.'
