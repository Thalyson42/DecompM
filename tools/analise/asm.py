#!/usr/bin/env python3
"""Desmonta um trecho do mugen.exe (PE32 x86) com capstone.  Uso: asm.py mugen.exe 0x441200 [0x441260]
Sem o segundo endereco, desmonta ate o primeiro 'ret' depois do inicio."""
import sys, pefile, capstone
pe = pefile.PE(sys.argv[1]); d = open(sys.argv[1], 'rb').read(); base = pe.OPTIONAL_HEADER.ImageBase
def off(v):
    for s in pe.sections:
        a = base + s.VirtualAddress
        if a <= v < a + max(s.Misc_VirtualSize, s.SizeOfRawData): return s.PointerToRawData + (v - a)
start = int(sys.argv[2], 16); end = int(sys.argv[3], 16) if len(sys.argv) > 3 else None
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
o = off(start)
for i in md.disasm(d[o:o + 0x4000], start):
    if end and i.address >= end: break
    print('%08x: %-7s %s' % (i.address, i.mnemonic, i.op_str))
    if not end and i.mnemonic == 'ret': break
