#!/usr/bin/env python3
"""VB1 p-code walker for rodent.exe.

Splits each RCDATA module into procedures, decodes p-code by following
control flow, using operand sizes and branch kinds derived from the
vbrun100 handlers (handlers.py), and writes:
  out/<mod>.lst      raw listing (proc, offset, opcode, operands)
  out/opcodes.txt    every opcode seen, with kind, size and handler disasm
"""
import os, sys, collections, json
from ne import NE
import handlers as H

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.join(HERE, '..', '..', 'rodents_revenge')
OUT = os.path.join(HERE, 'out')

# ---------------------------------------------------------------- modules

def load_modules():
    n = NE(os.path.join(GAME, 'rodent.exe'))
    mods = {}
    names = {2: 'RODENT2_FRM', 3: 'RODENT_GLB', 4: 'LEVEL_FRM', 5: 'PIX_FRM'}
    for t, i, o, l in n.resources:
        if t == 10 and i in names:
            mods[names[i]] = n.d[o:o+l]
    return mods

def name_table(b):
    """Identifier table: [word link][byte flag][byte len][name] runs."""
    k = 0; runs = []
    while k < len(b) - 4:
        ln = b[k+3]; nm = b[k+4:k+4+ln]
        ok = 2 <= ln < 40 and all((48 <= c < 58) or (65 <= c < 91) or (97 <= c < 123) or c == 95 for c in nm) and (65 <= nm[0] < 91 or 97 <= nm[0] < 123)
        if ok:
            runs.append((k, int.from_bytes(b[k:k+2], 'little'), b[k+2], nm.decode())); k += 4 + ln
        else:
            k += 1
    out = []
    for j, r in enumerate(runs):
        k, w, f, nm = r
        cont = (j > 0 and runs[j-1][0] + 4 + len(runs[j-1][3]) == k) or (j+1 < len(runs) and k + 4 + len(nm) == runs[j+1][0])
        if cont: out.append(r)
    return out

def first_header(b, names):
    after = names[-1][0] + 4 + len(names[-1][3]) if names else 0
    k = after
    while k < len(b) - 8:
        if b[k:k+2] == b'\xff\xff' and b[k+6:k+8] == b'\x18\x00':
            return k
        k += 1
    return None

def find_procs(b, start):
    """Walk procedure headers: ff ff len len2 18 00 ... code ... trailer."""
    procs = []
    pos = start
    while pos + 8 <= len(b):
        if b[pos+6:pos+8] != b'\x18\x00':
            break
        locals_ = int.from_bytes(b[pos:pos+2], 'little')
        ln = int.from_bytes(b[pos+2:pos+4], 'little')
        ln2 = int.from_bytes(b[pos+4:pos+6], 'little')
        frame = int.from_bytes(b[pos+6:pos+8], 'little')
        if ln < 8: break
        base = pos + 4                      # p-code offsets are relative to here
        end = base + ln
        procs.append(dict(hdr=pos, base=base, code=pos+8, end=end, ln=ln, ln2=ln2, frame=frame,
                          locals=locals_, trailer=b[end:end+5]))
        if b[end] == 0xfd: break            # last procedure of the module
        pos = end + 5                       # 5-byte trailer, then next header
    return procs

# ---------------------------------------------------------------- opcodes

class OpInfo:
    """Static facts about one opcode derived from its handler."""
    def __init__(self, op):
        self.op = op
        self.trace = H.trace(op)
        self.size, self.kind = self.classify(self.trace)
        self.targets = sorted({k for (k, e, rr) in self.trace if e == 'branch'})

    @staticmethod
    def classify(r):
        ends = {e for (k, e, rr) in r}
        nexts = {k for (k, e, rr) in r if e in ('next', 'jmpax')}
        term = {k for (k, e, rr) in r if e in ('ret', 'retf', 'iret', 'setsi')}
        calls = {k for (k, e, rr) in r if e == 'call-noret'}
        branches = {k for (k, e, rr) in r if e == 'branch'}
        if branches:
            size = max(branches) + 2
            if nexts and max(nexts) != size:
                return None, 'var'
            return size, ('jmp' if not nexts else 'jcc')
        if (2, 'varsi', ('w',)) in r and all(e in ('varsi', 'jmpind:cx', 'loop?', 'bad') for e in ends):
            return 2, 'reljmp'
        if len(nexts) == 1:
            return nexts.pop(), 'op'
        if nexts:
            return None, 'var'
        if term or calls:
            n = min(term | calls)
            if n == 0 and not calls:
                return 0, 'end'
            return n, 'call'
        return None, 'unknown'

OVERRIDE = {
    0x25c3: (2, 'op'), 0x25c6: (2, 'op'),      # string store [addr]
    0x135b: (0, 'end'), 0x135e: (0, 'end'),    # procedure end
    # runtime calls whose `call 0x3818; pop dx` trick confuses the tracer
    0x3960: (0, 'op'), 0x379e: (0, 'op'), 0x2d14: (0, 'op'), 0x1b7b: (0, 'op'), 0x48b0: (0, 'op'), 0x3072: (0, 'op'), 0x305d: (0, 'op'), 0x475e: (0, 'op'), 0x2e2d: (0, 'op'), 0x48a3: (0, 'op'), 0x2188: (0, 'op'), 0x22d5: (0, 'op'), 0x1a83: (0, 'op'), 0x1a7a: (0, 'op'), 0x15d4: (0, 'op'),
    # For/Next: init jumps to the Next block, whose trailing word points at the body
    0x16c9: (4, 'for'), 0x179d: (4, 'next'),
    # more far runtime routines
    0x2ef0: (0, 'op'), 0x473c: (0, 'op'), 0x3b28: (0, 'op'), 0x26af: (2, 'op'), 0x41d6: (2, 'op'),
    0x3118: (0, 'op'), 0x46cb: (0, 'op'), 0x48a3: (0, 'op'), 0x3072: (0, 'op'), 0x3408: (0, 'op'),
    # far runtime routines the tracer cannot follow to the end; operand
    # count is what was consumed before losing track (verified by resync)
    0x46c5: (0, 'op'), 0x47d6: (0, 'op'), 0x4770: (0, 'op'), 0x33fe: (0, 'op'),
    0x411d: (0, 'op'), 0x1658: (0, 'op'), 0x349d: (0, 'op'), 0x48b0: (0, 'op'),
    0x343f: (0, 'op'), 0x41a0: (2, 'op'), 0x3051: (0, 'op'), 0x2f4c: (0, 'op'),
}

def string_literal_size(code, pos):
    total = int.from_bytes(code[pos:pos+2], 'little')
    n = 2 + total
    return n + (n & 1)
SPECIAL = {0x2c92: string_literal_size}

_infos = {}
def info(op):
    if op not in _infos:
        oi = OpInfo(op)
        if op in OVERRIDE:
            oi.size, oi.kind = OVERRIDE[op]
        elif 0x2ed8 <= op < 0x2f28 and (op - 0x2ed8) % 12 == 0:
            oi.size, oi.kind = 0, 'op'         # long compares
        elif 0x16c9 <= op < 0x1720:
            oi.size, oi.kind = 4, 'for'        # For init (+Step variants)
        elif 0x179d <= op < 0x1800:
            oi.size, oi.kind = 4, 'next'       # Next (+Step variants)
        _infos[op] = oi
    return _infos[op]

# ---------------------------------------------------------------- decoding

def decode_proc(b, p, problems):
    """Follow control flow from the proc entry.  Returns {offset: (op, operands)}."""
    base, end = p['base'], p['end']
    insns = {}
    work = [p['code']]
    seen = set()
    while work:
        pos = work.pop()
        while pos + 2 <= end and pos not in seen:
            seen.add(pos)
            op = int.from_bytes(b[pos:pos+2], 'little')
            oi = info(op)
            if op in SPECIAL:
                n, kind = SPECIAL[op](b, pos+2), 'op'
            else:
                n, kind = oi.size, oi.kind
            if n is None:
                problems.append((p['hdr'], pos, op, kind, sorted(oi.trace)[:6]))
                break
            opnd = b[pos+2:pos+2+n]
            insns[pos] = (op, opnd)
            nxt = pos + 2 + n
            if kind == 'for':
                t = base + int.from_bytes(opnd[2:4], 'little')
                body = base + int.from_bytes(b[t:t+2], 'little')
                work.append(body); work.append(t + 2); break
            if kind == 'next':
                body = base + int.from_bytes(opnd[2:4], 'little')
                work.append(body); pos = nxt; continue
            if kind in ('jmp', 'jcc'):
                for t in oi.targets:
                    work.append(base + int.from_bytes(opnd[t:t+2], 'little'))
                if kind == 'jmp': break
                pos = nxt
            elif kind == 'reljmp':
                d = int.from_bytes(opnd, 'little', signed=True)
                pos = nxt + d
            elif kind == 'end':
                break
            else:
                pos = nxt
    return insns

def handler_snippet(op, count=24):
    lines = []
    a = op
    for _ in range(count):
        i = H.insn(a)
        if i is None: break
        ann = ''
        if i.mnemonic in ('lcall', 'ljmp'):
            ann = '   ; -> ' + str(H.far_target(2, i))
        lines.append(f'      {a:04x}  {i.mnemonic} {i.op_str}{ann}')
        if i.mnemonic in ('ret', 'retf', 'iret') or (i.mnemonic == 'jmp' and i.op_str == 'ax'):
            break
        a += i.size
    return lines

def main():
    H.load(os.path.join(GAME, 'vbrun100.dll'))
    os.makedirs(OUT, exist_ok=True)
    mods = load_modules()
    used = collections.Counter(); example = {}
    for mname, b in mods.items():
        names = name_table(b)
        hdr0 = first_header(b, names)
        problems = []
        with open(os.path.join(OUT, mname + '.lst'), 'w') as f:
            f.write(f'; module {mname} size={len(b):#x} first header={hdr0:#x}\n')
            if hdr0 is None: continue
            procs = find_procs(b, hdr0)
            f.write(f'; {len(procs)} procedures\n')
            for idx, p in enumerate(procs):
                f.write(f"\n; ---- proc {idx} hdr={p['hdr']:#06x} locals={p['locals']:#x} len={p['ln']:#x} len2={p['ln2']:#x} trailer={p['trailer'].hex()}\n")
                insns = decode_proc(b, p, problems)
                pos = p['code']
                while pos < p['end']:
                    if pos in insns:
                        op, opnd = insns[pos]
                        used[op] += 1; example.setdefault(op, (mname, idx, pos, opnd.hex()))
                        f.write(f'{pos-p["base"]:04x}  {op:04x}  {opnd.hex(" ")}\n')
                        pos += 2 + len(opnd)
                    else:
                        k = pos
                        while k < p['end'] and k not in insns: k += 1
                        f.write(f'{pos-p["base"]:04x}  ....  {b[pos:k].hex(" ")}\n')
                        pos = k
            f.write('\n; problems:\n')
            for pr in problems: f.write(f';  {pr}\n')
        print(mname, 'procs', len(procs), 'problems', len(problems))
    with open(os.path.join(OUT, 'opcodes.txt'), 'w') as f:
        for op, cnt in sorted(used.items()):
            oi = info(op)
            f.write(f'\n== {op:04x}  count={cnt} size={oi.size} kind={oi.kind} example={example[op]}\n')
            for l in handler_snippet(op): f.write(l + '\n')
    print('distinct opcodes', len(used))

if __name__ == '__main__':
    main()
