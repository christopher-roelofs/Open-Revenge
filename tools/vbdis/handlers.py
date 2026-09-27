"""Static analysis of vbrun100.dll p-code handlers.

Each p-code opcode is a near offset into segment 2 (the interpreter).  A
handler fetches its operands with `es: lodsw` / `es: lodsb` / `inc si` /
`add si,N`, then dispatches the next opcode with `es: lodsw; jmp ax`.
Branch opcodes load the new instruction pointer with `mov si, es:[si]`.

trace(op) explores every path through the handler, following near
calls/rets, far calls/jumps through NE relocations, and the type-dispatch
tables reached via `jmp bx`, and returns the set of possible operand layouts.
"""
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
from capstone.x86 import X86_OP_IMM, X86_OP_REG
from ne import NE
import functools

VBRUN = None
SEGS = {}       # segno -> bytes
RELOCS = {}     # segno -> {offset: ('seg', segno, off) | ('imp', name)}
md = Cs(CS_ARCH_X86, CS_MODE_16); md.detail = True

def load(path):
    global VBRUN
    VBRUN = NE(path)
    for s in VBRUN.segs:
        SEGS[s.idx] = s.data
        m = {}
        for (atype, rtype, off, a, b) in s.relocs:
            kind = rtype & 3
            if kind == 0:   val = ('seg', a, b)
            elif kind == 1: val = ('imp', f'{VBRUN.modules[a-1]}.{b}')
            elif kind == 2: val = ('imp', f'{VBRUN.modules[a-1]}.{VBRUN.import_name(b)}')
            else:           val = ('osfixup', a)
            o = off; seen = 0
            while o != 0xffff and o + 1 < len(s.data) and seen < 2000:
                m[o] = val
                if rtype & 4: break
                o = int.from_bytes(s.data[o:o+2], 'little'); seen += 1
        RELOCS[s.idx] = m

@functools.lru_cache(maxsize=None)
def insn(addr, seg=2):
    data = SEGS[seg]
    for i in md.disasm(data[addr:addr+16], addr):
        return i
    return None

def reloc(seg, off):
    return RELOCS.get(seg, {}).get(off)

def far_target(seg, i):
    """Resolve the seg:off of an ljmp/lcall using the relocation on its segment word."""
    r = reloc(seg, i.address + i.size - 2)
    if r and r[0] == 'seg':
        return (r[1], i.operands[0].imm if i.operands[0].type == X86_OP_IMM else r[2])
    return r

def is_es_lodsw(i): return i.mnemonic == 'lodsw' and 'es:' in i.op_str
def is_es_lodsb(i): return i.mnemonic == 'lodsb' and 'es:' in i.op_str

DISPATCH_BX = (2, 0x3b9d)   # pop ax; bx = cs:[bx + 2*ax - 2]; jmp bx  (type dispatch)

@functools.lru_cache(maxsize=None)
def trace(start, seg=2):
    """Explore every path through the handler at seg:start.

    Returns a set of (operand_bytes, end_kind, reads) tuples.
    """
    out = set()
    stack = [((seg, start), 0, (), (), ())]
    visited = set()
    steps = 0
    while stack:
        (sg, addr), n, reads, rets, regs = stack.pop()
        key = (sg, addr, n, rets, regs)
        if key in visited:
            continue
        visited.add(key)
        steps += 1
        if steps > 40000 or addr >= len(SEGS[sg]) or abs(n) > 64:
            out.add((n, 'loop?', reads)); continue
        if (sg, addr) == DISPATCH_BX:
            rd = dict(regs)
            if 'bx' in rd:
                tbl = rd['bx']
                for t in range(1, 9):
                    tgt = int.from_bytes(SEGS[2][tbl + 2*t - 2: tbl + 2*t], 'little')
                    stack.append(((2, tgt), n, reads, rets, ()))
                continue
            out.add((n, 'dispatch?', reads)); continue
        i = insn(addr, sg)
        if i is None: out.add((n, 'bad', reads)); continue
        nxt = addr + i.size
        m = i.mnemonic
        rd = dict(regs)
        if m == 'mov' and i.operands[0].type == X86_OP_REG and i.operands[1].type == X86_OP_IMM:
            rd[i.reg_name(i.operands[0].reg)] = i.operands[1].imm
        elif i.operands and i.operands[0].type == X86_OP_REG and m not in ('cmp', 'test', 'push'):
            rd.pop(i.reg_name(i.operands[0].reg), None)
        if m in ('pop', 'xchg', 'lodsw', 'lodsb', 'cwd', 'cwde', 'cbw', 'mul', 'imul', 'div', 'idiv'):
            for r in ('bx', 'cx', 'dx', 'ax', 'si', 'di'):
                if r in i.op_str or m in ('cwd', 'cwde', 'cbw', 'mul', 'imul', 'div', 'idiv'): rd.pop(r, None)
        regs2 = tuple(sorted(rd.items()))
        if is_es_lodsw(i):
            j = insn(nxt, sg)
            if j is not None and j.mnemonic == 'jmp' and j.op_str == 'ax':
                out.add((n, 'next', reads)); continue
            stack.append(((sg, nxt), n+2, reads+('w',), rets, regs2)); continue
        if is_es_lodsb(i):
            stack.append(((sg, nxt), n+1, reads+('b',), rets, regs2)); continue
        if m == 'inc' and i.op_str == 'si':
            stack.append(((sg, nxt), n+1, reads+('s',), rets, regs2)); continue
        if m == 'dec' and i.op_str == 'si':
            stack.append(((sg, nxt), n-1, reads+('-',), rets, regs2)); continue
        if m == 'add' and i.op_str.startswith('si, ') and i.operands[1].type == X86_OP_IMM:
            k = i.operands[1].imm
            stack.append(((sg, nxt), n+k, reads+('s'*k,), rets, regs2)); continue
        if m == 'add' and i.op_str.startswith('si, ') and i.operands[1].type != X86_OP_IMM:
            out.add((n, 'varsi', reads)); continue
        if m == 'mov' and i.op_str.startswith('si, word ptr es:[si'):
            out.add((n, 'branch', reads)); continue
        if m == 'mov' and i.op_str.startswith('si, '):
            out.add((n, 'setsi', reads)); continue
        if m == 'iret':
            out.add((n, m, reads)); continue
        if m in ('ret', 'retf'):
            if rets:
                stack.append((rets[-1], n, reads, rets[:-1], regs2))
            else:
                out.add((n, 'ret', reads))
            continue
        if m == 'jmp':
            op = i.operands[0]
            if op.type == X86_OP_IMM:
                stack.append(((sg, op.imm), n, reads, rets, regs2))
            elif i.op_str == 'ax':
                out.add((n, 'jmpax', reads))
            elif i.op_str in rd:
                stack.append(((sg, rd[i.op_str]), n, reads, rets, regs2))
            else:
                out.add((n, 'jmpind:' + i.op_str, reads))
            continue
        if m in ('ljmp', 'lcall'):
            t = far_target(sg, i)
            if isinstance(t, tuple) and t[0] not in ('imp', 'osfixup') and t[0] in SEGS:
                if m == 'ljmp':
                    stack.append((t, n, reads, rets, ()))
                elif len(rets) < 10:
                    stack.append((t, n, reads, rets + ((sg, nxt),), ()))
                else:
                    out.add((n, 'call-deep', reads))
            else:
                if m == 'lcall':
                    stack.append(((sg, nxt), n, reads, rets, ()))   # API import returns
                else:
                    out.add((n, 'ljmp:' + str(t), reads))
            continue
        if m.startswith('j') or m in ('loop', 'loope', 'loopne', 'jcxz'):
            op = i.operands[0]
            stack.append(((sg, nxt), n, reads, rets, regs2))
            if op.type == X86_OP_IMM:
                stack.append(((sg, op.imm), n, reads, rets, regs2))
            continue
        if m == 'call' and i.operands[0].type == X86_OP_IMM:
            if len(rets) < 10:
                stack.append(((sg, i.operands[0].imm), n, reads, rets + ((sg, nxt),), regs2))
            else:
                out.add((n, 'call-deep', reads))
            continue
        if m == 'call':
            stack.append(((sg, nxt), n, reads, rets, ())); continue
        stack.append(((sg, nxt), n, reads, rets, regs2))
    return out

def meta(op):
    return int.from_bytes(SEGS[2][op-2:op], 'little')

if __name__ == '__main__':
    import sys
    load(sys.argv[1])
    for a in sys.argv[2:]:
        op = int(a, 16)
        print(hex(op), sorted(trace(op)))
