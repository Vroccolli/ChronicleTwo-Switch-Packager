.text
.align 4
.global ctFiberAbiCheck
.type ctFiberAbiCheck, %function
// Runs the supplied yielding function with distinctive callee-register values.
ctFiberAbiCheck:
    sub sp, sp, #192
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]
    stp d8, d9, [sp, #96]
    stp d10, d11, [sp, #112]
    stp d12, d13, [sp, #128]
    stp d14, d15, [sp, #144]
    mrs x2, fpcr
    str x2, [sp, #160]
    mrs x2, fpsr
    str x2, [sp, #168]
    str x0, [sp, #176]
    mov x19, #0x19
    mov x20, #0x20
    mov x21, #0x21
    mov x22, #0x22
    mov x23, #0x23
    mov x24, #0x24
    mov x25, #0x25
    mov x26, #0x26
    mov x27, #0x27
    mov x28, #0x28
    mov x29, #0x29
    mov x2, #0x88
    fmov d8, x2
    mov x2, #0x99
    fmov d9, x2
    mov x2, #0xaa
    fmov d10, x2
    mov x2, #0xbb
    fmov d11, x2
    mov x2, #0xcc
    fmov d12, x2
    mov x2, #0xdd
    fmov d13, x2
    mov x2, #0xee
    fmov d14, x2
    mov x2, #0xff
    fmov d15, x2
    ldr x16, [sp, #176]
    blr x16
    mov x0, #0
.macro check_int reg, value
    cmp \reg, #\value
    cset x2, ne
    add x0, x0, x2
.endm
    check_int x19, 0x19
    check_int x20, 0x20
    check_int x21, 0x21
    check_int x22, 0x22
    check_int x23, 0x23
    check_int x24, 0x24
    check_int x25, 0x25
    check_int x26, 0x26
    check_int x27, 0x27
    check_int x28, 0x28
    check_int x29, 0x29
.macro check_fp reg, value
    fmov x3, \reg
    check_int x3, \value
.endm
    check_fp d8, 0x88
    check_fp d9, 0x99
    check_fp d10, 0xaa
    check_fp d11, 0xbb
    check_fp d12, 0xcc
    check_fp d13, 0xdd
    check_fp d14, 0xee
    check_fp d15, 0xff
    mrs x2, fpcr
    ldr x3, [sp, #160]
    cmp x2, x3
    cset x2, ne
    add x0, x0, x2
    ldp d8, d9, [sp, #96]
    ldp d10, d11, [sp, #112]
    ldp d12, d13, [sp, #128]
    ldp d14, d15, [sp, #144]
    ldr x2, [sp, #160]
    msr fpcr, x2
    ldr x2, [sp, #168]
    msr fpsr, x2
    ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    add sp, sp, #192
    ret
.size ctFiberAbiCheck, .-ctFiberAbiCheck
.section .note.GNU-stack,"",%progbits
