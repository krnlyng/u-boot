
    .globl lowlevel_init
lowlevel_init:
    mov pc, lr

.global resetIPOD
.global hangIPOD
.type resetIPOD, %function
.type hangIPOD, %function
resetIPOD:
    msr cpsr_c, #0xd3
    mov r0, #0x110000
    add r0, r0, #0xff
    add r1, r0, #0xa00
    mov r2, #0x3c800000
    str r1, [r2]
    mov r1, #0xff0
    str r1, [r2,#4]
    str r0, [r2]
hangIPOD:
    msr cpsr_c, #0xd3
    mcr p15, 0, r0,c7,c0,4
    b   hangIPOD
.size resetIPOD, .-resetIPOD
.size hangIPOD, .-hangIPOD


