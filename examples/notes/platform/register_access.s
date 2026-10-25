.syntax unified
.cpu cortex-m4
.thumb
.text

.global pa5_high_asm
.type pa5_high_asm, %function
.thumb_func
pa5_high_asm:
    movw r0, #0
    movt r0, #0x4002
    movs r1, #32
    str r1, [r0, #24]
    bx lr
.size pa5_high_asm, .-pa5_high_asm

.global pa5_input_asm
.type pa5_input_asm, %function
.thumb_func
pa5_input_asm:
    ldr r1, =0x40020000
    ldr r0, [r1, #16]
    lsrs r0, r0, #5
    and r0, r0, #1
    bx lr
.size pa5_input_asm, .-pa5_input_asm
.ltorg
