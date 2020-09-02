.section .text.start
.global secondary_main
.type   secondary_main, @function

secondary_main:
  addi sp, sp, -16
#if __riscv_xlen == 32
  sw ra, 4(sp)
#else
  sd ra, 8(sp)
#endif
  csrr t0, mhartid
  la t1, __metal_boot_hart
  beq t0, t1, 3f
1:
  // enable software interrupts
  csrrs x0, mie, 0x8
  wfi
  la t2, _metal_exec_array
#if __riscv_xlen == 32
  slli t1, t0, 2
#else
  slli t1, t0, 3
#endif
  add t2, t1, t2
  bnez t2, 2f
  j 1b
2:
  lw t2, 0(t2)
  jalr t2
  j 1b
3:
  call main
#if __riscv_xlen == 32
  lw ra, 4(sp)
#else
  ld ra, 8(sp)
#endif
  addi sp, sp, 16
  ret
