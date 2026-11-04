// Context layout is checked by static_asserts in fiber.hpp.
.text
.align 4
.global ctFiberSwap
.type ctFiberSwap, %function
ctFiberSwap:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]
    mov x2, sp
    str x2, [x0, #96]
    mrs x2, fpcr
    str x2, [x0, #104]
    mrs x2, fpsr
    str x2, [x0, #112]
    stp q8, q9, [x0, #128]
    stp q10, q11, [x0, #160]
    stp q12, q13, [x0, #192]
    stp q14, q15, [x0, #224]
    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]
    ldr x2, [x1, #104]
    msr fpcr, x2
    ldr x2, [x1, #112]
    msr fpsr, x2
    ldp q8, q9, [x1, #128]
    ldp q10, q11, [x1, #160]
    ldp q12, q13, [x1, #192]
    ldp q14, q15, [x1, #224]
    ldr x2, [x1, #96]
    mov sp, x2
    ret
.size ctFiberSwap, .-ctFiberSwap

.align 4
.global ctFiberStart
.type ctFiberStart, %function
ctFiberStart:
    blr x19
    bl ctFiberUnexpectedReturn
    brk #0
.size ctFiberStart, .-ctFiberStart
.section .note.GNU-stack,"",%progbits
