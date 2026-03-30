! Test that ld -r preserves addends on R_IMM32 relocations.
! After ld -r, the relocation for _ps1nod+12 must still carry
! the +12 addend in r_offset.
	.text
	.global _start
_start:
	ldl	rr2, _ps1nod+12
