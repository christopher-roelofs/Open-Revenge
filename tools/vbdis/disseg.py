"""Disassemble one vbrun100 segment with reloc annotations."""
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
from ne import NE

def reloc_map(n, seg):
    m = {}
    for (atype, rtype, off, a, b) in seg.relocs:
        kind = rtype & 3
        if kind == 0:   txt = f'seg{a}:{b:04x}' if a != 0xff else f'entry{b}'
        elif kind == 1: txt = f'{n.modules[a-1]}.{b}'
        elif kind == 2: txt = f'{n.modules[a-1]}.{n.import_name(b)}'
        else:           txt = f'osfixup{a}'
        # follow chain
        o = off
        seen = 0
        while o != 0xffff and o < len(seg.data) and seen < 1000:
            m[o] = txt
            if rtype & 4: break  # additive
            nxt = int.from_bytes(seg.data[o:o+2], 'little')
            o = nxt; seen += 1
        m[off] = txt
    return m

def dis(n, segidx, start=0, end=None):
    seg = n.segs[segidx-1]
    rm = reloc_map(n, seg)
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    md.skipdata = True
    code = seg.data[start:end]
    for ins in md.disasm(code, start):
        ann = ''
        for k in range(ins.address, ins.address+ins.size):
            if k in rm: ann = '   ; ' + rm[k]; break
        yield ins, ann

if __name__ == '__main__':
    n = NE(sys.argv[1]); s = int(sys.argv[2])
    for ins, ann in dis(n, s):
        print(f'{s}:{ins.address:04x}  {ins.bytes.hex():<16} {ins.mnemonic} {ins.op_str}{ann}')
