#!/usr/bin/env python3
"""Extrai do mugen.exe (1.1b1, PE32 x86) a cadeia de comparacoes "nome do trigger -> opcode".

No exe, o tokenizer das expressoes compara cada identificador com uma lista de nomes (stricmp) e devolve
um opcode em EAX; todos os casos saltam para 0x00454AA0. Este script desmonta a regiao e coleta os pares.

Uso:  python3 extrair_triggers.py caminho/mugen.exe        (precisa de: pip install capstone pefile)
Resultado heuristico: confira os pares suspeitos (nomes diferentes com o mesmo opcode) no desassemblado.
"""
import sys, pefile, capstone

pe = pefile.PE(sys.argv[1]); d = open(sys.argv[1], 'rb').read(); base = pe.OPTIONAL_HEADER.ImageBase
def va2off(v):
    for s in pe.sections:
        a = base + s.VirtualAddress
        if a <= v < a + max(s.Misc_VirtualSize, s.SizeOfRawData): return s.PointerToRawData + (v - a)
def cstr(v):
    o = va2off(v)
    if o is None: return None
    e = d.find(b'\x00', o); s = d[o:e]
    return s.decode('latin-1') if 0 <= len(s) < 40 and all(32 <= c < 127 for c in s) else None

START, END = 0x4520a0, 0x454ab0
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
ins = [i for i in md.disasm(d[va2off(START):va2off(START) + 0x3000], START) if i.address <= END]
cur = None; since = 0; out = []
for i in ins:
    if i.mnemonic == 'push' and i.op_str.startswith('0x'):
        v = int(i.op_str, 16)
        if 0x4f0000 <= v < 0x500000 and cstr(v) is not None: cur = (cstr(v), i.address); since = 0
    elif cur:
        since += 1
        if i.mnemonic == 'mov' and i.op_str.startswith('eax, 0x') and since <= 6:
            out.append((cur[0], int(i.op_str.split(',')[1], 16), cur[1])); cur = None
        elif since > 8: cur = None
for nome, op, addr in out: print('%#x\t%s\t%d' % (addr, nome, op))
