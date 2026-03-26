#name: Z8001 short segmented address relaxation
#source: relax-seg.s -z8001
#ld: --relax -T relax-seg.ld -mz8001 -e 0
#objdump: -dr

.*:     file format coff-z8k


Disassembly of section \.text:

00010000 <_start>:
   10000:	8d07           	nop\s*
   10002:	5e08 0100      	jp	t,0x1000000
   10006:	5f00 0100      	call	0x1000000
   1000a:	6101 0200      	ld	r1,0x2000000
   1000e:	5e08 8100 0106 	jp	t,0x1000106
   10014:	8d07           	nop\s*
#...

00010106 <far_label>:
   10106:	8d07           	nop\s*
