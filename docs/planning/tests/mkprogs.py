#!/usr/bin/env python3
#
# Writes the hand-assembled 68k test programs used by Tests.md into the
# directory given as the only argument. All are linked for 0x40000, the
# quickload address, and use no firmware calls.
#
#   hello.bin   prints "hello, world" on DUART channel A, then loops
#   echo.bin    prints "hello, world", then echoes channel A input
#   berr.bin    causes five bus errors, copies each exception frame
#               (24 bytes) to 0x3000 + 0x20*n
#   flash1.bin  sector-erases flash sector 127, programs 0x1234/0xABCD,
#               programs 0x00FF over 0x1234; result long at 0x3000,
#               chip IDs at 0x3004, 0xCAFEF00D at 0x3008 when done
#   flash2.bin  chip-erases both flash chips; first long of the flash at
#               0x3000, 0xCAFEF00D at 0x3008 when done

import struct
import sys


def w(*xs):
	return b''.join(struct.pack('>H', x) for x in xs)


def hello():
	code  = w(0x41FA, 0x001A)                   # 0:  lea (msg,pc),a0
	code += w(0x1018, 0x6712)                   # 4:  loop: move.b (a0)+,d0; beq.s done
	code += w(0x0839, 0x0002, 0x00F0, 0x0003)   # 8:  wait: btst #2,$f00003 (SRA TxRDY)
	code += w(0x67F6)                           # 16: beq.s wait
	code += w(0x13C0, 0x00F0, 0x0007)           # 18: move.b d0,$f00007 (THRA)
	code += w(0x60EA, 0x60FE)                   # 24: bra.s loop; done: bra.s done
	return code + b'hello, world\r\n\0'         # 28: msg


def echo():
	code  = w(0x46FC, 0x2700)                   # 0:  move.w #$2700,sr (keep the firmware ISR off the DUART)
	code += w(0x41FA, 0x003A)                   # 4:  lea (msg,pc),a0
	code += w(0x1018, 0x6712)                   # 8:  loop: move.b (a0)+,d0; beq.s echo
	code += w(0x0839, 0x0002, 0x00F0, 0x0003)   # 12: wait: btst #2,$f00003 (TxRDY)
	code += w(0x67F6)                           # 20: beq.s wait
	code += w(0x13C0, 0x00F0, 0x0007)           # 22: move.b d0,$f00007
	code += w(0x60EA)                           # 28: bra.s loop
	code += w(0x0839, 0x0000, 0x00F0, 0x0003)   # 30: echo: btst #0,$f00003 (RxRDY)
	code += w(0x67F6)                           # 38: beq.s echo
	code += w(0x1039, 0x00F0, 0x0007)           # 40: move.b $f00007,d0 (RHRA)
	code += w(0x0839, 0x0002, 0x00F0, 0x0003)   # 46: wait2: btst #2,$f00003
	code += w(0x67F6)                           # 54: beq.s wait2
	code += w(0x13C0, 0x00F0, 0x0007)           # 56: move.b d0,$f00007
	code += w(0x60DE)                           # 62: bra.s echo
	return code + b'hello, world\r\n\0'         # 64: msg


def berr():
	code  = w(0x46FC, 0x2700)                   # move.w #$2700,sr
	code += w(0x21FC, 0x0004, 0x0100, 0x0008)   # move.l #$40100,$8.w (bus error vector)
	code += w(0x2C4F)                           # movea.l sp,a6
	tests = [
		w(0x1039, 0x0020, 0x0001),              # move.b $200001,d0  (RAM hole, odd byte read)
		w(0x33C0, 0x0020, 0x0000),              # move.w d0,$200000  (RAM hole, word write)
		w(0x1039, 0x00F0, 0x0021),              # move.b $f00021,d0  (first expansion range)
		w(0x3039, 0x00F8, 0x0060),              # move.w $f80060,d0  (second expansion range)
		w(0x3039, 0x0200, 0x0000),              # move.w $2000000,d0 (above 16MB, 020/030 only)
	]
	for i, t in enumerate(tests):
		code += w(0x49F8, 0x3000 + 0x20 * i)    # lea slot.w,a4
		code += w(0x4BFA, 0x0008)               # lea (next,pc),a5
		code += t
	code += w(0x60FE)                           # bra.s *
	code = code.ljust(0x100, b'\0')             # handler at 0x40100:
	code += w(0x28DF) * 6                       #   move.l (sp)+,(a4)+ x6
	code += w(0x2E4E, 0x4ED5)                   #   movea.l a6,sp; jmp (a5)
	return code


# Flash: word writes reach both chips at once. Chip address n is CPU
# address 0xE00000 + 2n, so the unlock addresses 0x5555/0x2AAA are
# 0xE0AAAA/0xE05554 and sector 127 (chip 0x7F000) is 0xEFE000. Status is
# polled at the chip base until two reads agree (toggle bit stopped).
SETUP = (w(0x46FC, 0x2700)                      # move.w #$2700,sr
	+ w(0x207C, 0x00E0, 0xAAAA)                 # movea.l #$E0AAAA,a0
	+ w(0x227C, 0x00E0, 0x5554)                 # movea.l #$E05554,a1
	+ w(0x247C, 0x00EF, 0xE000)                 # movea.l #$EFE000,a2
	+ w(0x267C, 0x00E0, 0x0000)                 # movea.l #$E00000,a3
	+ w(0x323C, 0xAAAA, 0x343C, 0x5555))        # move.w #$AAAA,d1; move.w #$5555,d2
UNLOCK = w(0x3081, 0x3282)                      # move.w d1,(a0); move.w d2,(a1)
POLL = w(0x3013, 0xB053, 0x66FA)                # move.w (a3),d0; cmp.w (a3),d0; bne.s *-4
DONE = w(0x21FC, 0xCAFE, 0xF00D, 0x3008, 0x60FE)  # move.l #$CAFEF00D,$3008.w; bra.s *


def cmd(c):
	return w(0x30BC, c)                         # move.w #c,(a0)


def flash1():
	return (SETUP
		+ UNLOCK + cmd(0x8080) + UNLOCK + w(0x34BC, 0x3030) + POLL        # sector erase
		+ UNLOCK + cmd(0xA0A0) + w(0x34BC, 0x1234) + POLL                 # (a2) = 1234
		+ UNLOCK + cmd(0xA0A0) + w(0x357C, 0xABCD, 0x0002) + POLL         # 2(a2) = ABCD
		+ UNLOCK + cmd(0xA0A0) + w(0x34BC, 0x00FF) + POLL                 # (a2) &= 00FF
		+ w(0x21D2, 0x3000)                                               # move.l (a2),$3000.w
		+ UNLOCK + cmd(0x9090) + w(0x21F9, 0x00E0, 0x0000, 0x3004) + cmd(0xF0F0)  # IDs
		+ DONE)


def flash2():
	return (SETUP
		+ UNLOCK + cmd(0x8080) + UNLOCK + cmd(0x1010) + POLL              # chip erase
		+ w(0x21D3, 0x3000)                                               # move.l (a3),$3000.w
		+ DONE)


if __name__ == '__main__':
	out = sys.argv[1]
	for name, fn in (('hello', hello), ('echo', echo), ('berr', berr), ('flash1', flash1), ('flash2', flash2)):
		with open(f'{out}/{name}.bin', 'wb') as f:
			f.write(fn())
