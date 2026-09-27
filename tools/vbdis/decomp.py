#!/usr/bin/env python3
"""Pseudo-VB printer for the p-code walked by vbdis.py.

Semantics per opcode were derived from the vbrun100 handler disassembly
(see out/opcodes.txt).  Expressions are rebuilt with a symbolic stack; when
the stack does not line up the raw opcode is printed with the stack dump so
the table can be corrected.
"""
import os, sys, struct, collections
import vbdis as V
import handlers as H

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'out')

# ---------------------------------------------------------------- symbols

# procedure names in handle order for RODENT2.FRM (from the global module's
# routine list); handle = 0x70 + 0x2e*(k-1) for proc k
FRM_PROCS = ['<decl>', 'AppSize', 'AppInit', 'MyRand', 'Form_Load', 'Form_Resize', 'Form_Paint',
    'fld_KeyDown', 'zGame_Click', 'uNew_Click', 'uPause_Click', 'uHigh_Click', 'uExit_Click',
    'uLevel_Click', 'uSize_Click', 'uSpeed_Click', 'uIndex_Click', 'uPlay_Click', 'uCmds_Click',
    'uUsing_Click', 'uAbout_Click', 'LevelNew', 'LevelDraw', 'GuyKill', 'GuyNew', 'pixGuy_Paint',
    'GuyPaint', 'GuyMove', 'BadIFindBad', 'FPushBlock', 'BadFOpen', 'BadKill', 'BadNew', 'BadMove',
    'BadFMoveKat', 'BadMoveYarn', 'YarnMove', 'YarnFillDir', 'YarnDirFFire', 'TimeReset', 'TimeAdd',
    'pixTime_Paint', 'ScoreReset', 'ScoreAdd', 't_timer', 'ModeSet', 'SetTimer', 'MinMod', 'LblDo',
    'Form_Unload']
FUNCTIONS = {'MyRand', 'BadIFindBad', 'FPushBlock', 'BadFOpen', 'BadFMoveKat', 'YarnFillDir',
             'YarnDirFFire', 'MinMod', 'FChkWepVers', 'GetPrivateProfileInt', 'GetDeviceCaps',
             'FldGet', 'WepScore', 'WritePrivateProfileString'}

def handle_name(h):
    if h in HANDLES: return HANDLES[h]
    if h >= 0x70 and (h - 0x70) % 0x2e == 0:
        k = (h - 0x70) // 0x2e + 1
        if k < len(FRM_PROCS): return FRM_PROCS[k]
    return f'proc_{h:04x}'

SYMS = {}       # data address -> name (filled from symbols file if present)
HANDLES = {}    # DLL declare handle -> name
FIELDS = {0x120: 'x', 0x128: 'y', 0x130: 'ch', 0x138: 'type', 0x140: 'dir'}   # GUYTYPE
UNKNOWN = collections.Counter()
def sym(addr, prefix='G'):
    return SYMS.get(addr, f'{prefix}{addr:X}')

def load_syms():
    p = os.path.join(HERE, 'symbols.txt')
    if os.path.exists(p):
        table = SYMS
        for line in open(p):
            line = line.split('#')[0].strip()
            if not line: continue
            if line == '[handles]': table = HANDLES; continue
            if line == '[data]': table = SYMS; continue
            a, n = line.split()
            table[int(a, 16)] = n

# ---------------------------------------------------------------- op table
# kind: push (fmt with {o}=operand), bin (sym), un (fmt {a}), store (fmt {v}),
#       nop, stmt, jcc, jmp, custom
OPS = {
    # statement markers / type tags (no runtime effect)
    0x3651: ('stmt',), 0x364b: ('stmt',), 0x365e: ('stmt',), 0x363b: ('stmt',),
    0x3635: ('stmt',), 0x3625: ('stmt',), 0x361f: ('stmt',), 0x360f: ('stmt',),
    0x3609: ('stmt',), 0x366b: ('stmt',), 0x35c7: ('stmt',), 0x3705: ('stmt',),
    0x3747: ('nop',), 0x374a: ('nop',), 0x3753: ('nop',), 0x3721: ('nop',), 0x3733: ('nop',),
    0x373f: ('nop',), 0x2a5f: ('nop',), 0x2b7d: ('nop',), 0x2b83: ('nop',), 0x411d: ('nop',),
    # constants
    0x2bfb: ('push', '0'), 0x2c03: ('push', '1'), 0x2c0e: ('push', '2'), 0x2c14: ('push', '3'),
    0x2c1a: ('push', '4'), 0x2c20: ('push', '5'), 0x2c26: ('push', '6'), 0x2c2c: ('push', '7'),
    0x2c32: ('push', '8'), 0x2c38: ('push', '9'), 0x2c3e: ('push', '10'),
    0x2c47: ('pushimm',), 0x2c4a: ('pushimm',),
    0x2c72: ('fconst',), 0x2c92: ('pushstr',), 0x2baa: ('fpush', '1#'), 0x3b28: ('fpush', 'Timer'),
    0x142b: ('push', '1'), 0x1485: ('push', '2'), 0x15cc: ('push', '6'),
    # variable access (module data)
    0x23ab: ('pushvar', 'G'), 0x23c7: ('pushvar', 'G'), 0x23d8: ('pushvar', 'G'), 0x23fe: ('fvar', 'G'),
    0x25e9: ('store', 'G'), 0x25de: ('store', 'G'), 0x25f7: ('store', 'G'), 0x2608: ('store', 'G'),
    0x262d: ('fstore', 'G'), 0x25c3: ('store', 'G'), 0x25c6: ('store', 'G'),
    0x37dd: ('pushaddr', 'G'),
    # locals (operand points at a frame-offset word)
    0x2498: ('pushvar', 'L'), 0x24ca: ('pushvar', 'L'), 0x264d: ('store', 'L'),
    0x2824: ('pushaddr', 'L'),
    0x240d: ('pushref', 'L'),                    # deref ByRef parameter
    # global module (RODENT.GLB) data via slot
    0x232f: ('pushvar', 'X'), 0x234b: ('pushvar', 'X'), 0x2505: ('store', 'X'),
    # arrays
    0x0118: ('arr1',), 0x00e6: ('arrload',), 0x01cf: ('arrstore',), 0x077a: ('arraddr',),
    0x432e: ('field',), 0x43f1: ('fieldstore',),
    # integer arithmetic / logic
    0x2cb5: ('bin', '+'), 0x2cc3: ('bin', '-'), 0x2cd1: ('bin', '*'), 0x2ce1: ('bin', '\\'),
    0x2cfa: ('bin', 'Mod'), 0x2ced: ('un', '-{a}'), 0x2d22: ('un', 'Abs({a})'),
    0x2da6: ('un', 'Not {a}'), 0x2db1: ('bin', 'And'), 0x2dbc: ('bin', 'Or'),
    0x2df7: ('un', 'Sgn({a})'), 0x2e6a: ('un', 'Not& {a}'),
    0x2ed8: ('bin', '<'), 0x2ee4: ('bin', '>'), 0x2ef0: ('bin', '>='), 0x2efc: ('bin', '<='),
    0x2f08: ('bin', '='), 0x2f14: ('bin', '<>'),
    0x2d34: ('bin', '='), 0x2d37: ('bin', '='), 0x2d4a: ('bin', '<>'),
    0x2d5d: ('bin', '<='), 0x2d70: ('bin', '>='), 0x2d83: ('bin', '>'), 0x2d96: ('bin', '<'),
    # conversions / floating point
    0x140e: ('i2f', 'CDbl({a})'), 0x145e: ('un', 'CCur({a})'), 0x1490: ('f2i', 'CInt({a})'),
    0x13fe: ('un', 'CLng({a})'), 0x1658: ('fun', 'Rnd({a})'),
    0x2f31: ('fbin', '-'), 0x2f3a: ('fbin', '*'), 0x2f43: ('fbin', '/'), 0x2f4c: ('fbin', '^'),
    0x2f28: ('fbin', '+'), 0x2bb3: ('fpush', '2'),
    0x3008: ('fcmp', '>'), 0x3041: ('fcmp', '<'), 0x3051: ('bin', '+@'), 0x3072: ('un', 'Int@({a})'),
    0x146e: ('f2i', 'CCur({a})'), 0x159b: ('un', 'CLng@({a})'), 0x15af: ('i2f', 'CDbl({a})'),
    0x46b4: ('randomize',), 0x2e2d: ('bin', '*&'), 0x48a3: ('bin', 'Format$'),
    0x349d: ('vcoerce',), 0x31ab: ('caserange',), 0x144a: ('i2f', 'CDbl({a})'), 0x2f81: ('fun', 'Sqr({a})'),
    0x14a4: ('f2i', 'CLng({a})'), 0x2e17: ('bin', '-&'), 0x2e04: ('bin', '+&'), 0x1436: ('un', 'CInt({a})'),
    0x3526: ('bin', '*?'), 0x48b0: ('bin', 'Format$'), 0x269c: ('fstore', 'L'), 0x24f4: ('fvar', 'L'),
    0x4770: ('un', 'Str$({a})'), 0x473c: ('un', 'LTrim$({a})'), 0x2188: ('textwidth',),
    0x1513: ('un', '{a}'), 0x151b: ('un', '{a}'), 0x152b: ('un', '{a}'),

    0x33fe: ('bin', '+?'), 0x343f: ('bin', '=?'),
    0x0b76: ('drop2',), 0x0b79: ('drop2',),
    # control flow
    0x18e4: ('jmp',), 0x18ea: ('jmp',), 0x18ed: ('jmp',), 0x18f0: ('jmp',), 0x18f9: ('jmp',),
    0x2a71: ('jmp',), 0x3f7f: ('reljmp',), 0x3f82: ('reljmp',),
    0x2949: ('jcc0',), 0x297e: ('jcc',), 0x2987: ('jcc',), 0x298a: ('jcc',), 0x298d: ('jcc',),
    0x0b33: ('case',), 0x0afd: ('casejmp',),
    0x16c9: ('for',), 0x16dd: ('forstep',), 0x179d: ('next',), 0x17ca: ('next',),
    0x24b7: ('pushvar', 'L'), 0x24e3: ('fvar', 'L'), 0x2630: ('fstore', 'G'), 0x265d: ('store', 'L'),
    0x2689: ('fstore', 'L'), 0x26d7: ('store', 'G'), 0x27e8: ('pushaddr', 'G'), 0x2a68: ('jmp',),
    0x2c53: ('pushcur',), 0x2c85: ('pushlng',), 0x015f: ('arrstore1',), 0x1523: ('un', '{a}'),
    0x46cb: ('un', 'Chr$({a})'), 0x3118: ('bin', '&'),
    0x4734: ('nop',), 0x305d: ('bin', '-@'), 0x475e: ('bin', '&'),
    0x2d14: ('bin', '\\&'), 0x3408: ('bin', '-?'), 0x0cd5: ('calldll',),

    0x15fa: ('nop',), 0x162b: ('push', 'Err'), 0x1420: ('un', 'Rt1420({a})'),
    0x26af: ('strtmp',), 0x2836: ('strtmp',),
    0x3fd6: ('ret',), 0x135b: ('ret',), 0x135e: ('ret',), 0x379e: ('end',),
    0x4ae8: ('onerror',),
    # calls
    0x3d76: ('call',), 0x3d49: ('callslot',), 0x0cd5: ('calldll',),
    0x3802: ('rtcall',), 0x3960: ('un', 'CtlRef({a})'), 0x1a83: ('rt14',), 0x1a7a: ('rt14',),
    0x3795: ('push', 'Screen'), 0x15d4: ('rtstack',),
    0x3834: ('propgetv',), 0x385f: ('propget',), 0x38de: ('propset',), 0x388a: ('propsetv',),
    0x38e5: ('propcall',), 0x3935: ('propcall',),
    0x0ba3: ('tmpbuf',), 0x4155: ('setlen',), 0x41a0: ('nop',), 0x41d6: ('nop',), 0x15d4: ('nop',),
}

# ---------------------------------------------------------------- decoder

def fmt_operand(op, opnd):
    return opnd.hex(' ')

def decode_module(mname, b):
    names = V.name_table(b)
    hdr0 = V.first_header(b, names)
    procs = V.find_procs(b, hdr0)
    return procs

def local_name(b, addr):
    """Locals are addressed through a word in the data area holding the frame offset."""
    return sym(addr, 'L')

def pretty_proc(mname, b, idx, p, out):
    problems = []
    insns = V.decode_proc(b, p, problems)
    base = p['base']
    name = FRM_PROCS[idx] if mname == 'RODENT2_FRM' and idx < len(FRM_PROCS) else f'{mname}_{idx}'
    out.append(f'\n\'==== {name}  (proc {idx}, hdr {p["hdr"]:#x}, locals {p["locals"]:#x})')
    # labels = branch targets
    labels = set()
    for pos, (op, opnd) in insns.items():
        oi = V.info(op)
        for t in oi.targets:
            labels.add(int.from_bytes(opnd[t:t+2], 'little'))
        if op in (0x16c9, 0x179d):
            pass
    stack = []; fst = []
    casev = [None]; case_at = {}
    def pop():
        return stack.pop() if stack else '?'
    def fpop():
        return fst.pop() if fst else '?#'
    def emit(s):
        out.append(f'    {s}')
    def flush(pos):
        if stack or fst:
            out.append(f'        \' stack: {stack} fpu: {fst}')
            stack.clear(); fst.clear()
    for pos in sorted(insns):
        rel = pos - base
        op, opnd = insns[pos]
        if rel in labels:
            out.append(f'L{rel:04x}:')
            if rel in case_at and case_at[rel] is not None:
                stack.clear(); stack.append(case_at[rel]); stack.append(case_at[rel])
        entry = OPS.get(op)
        if entry is None and 0x16c9 <= op < 0x1720: entry = ('forstep',) if op != 0x16c9 else ('for',)
        if entry is None and 0x179d <= op < 0x1800: entry = ('next',)
        w = int.from_bytes(opnd[0:2], 'little') if len(opnd) >= 2 else None
        w2 = int.from_bytes(opnd[2:4], 'little') if len(opnd) >= 4 else None
        if entry is None:
            UNKNOWN[op] += 1
            emit(f'?? {op:04x} {opnd.hex(" ")}   \' stack={stack}')
            continue
        k = entry[0]
        if k == 'stmt':
            flush(pos)
        elif k == 'nop':
            pass
        elif k == 'push':
            stack.append(entry[1])
        elif k == 'pushimm':
            v = w if w < 0x8000 else w - 0x10000
            stack.append(str(v))
        elif k == 'fconst':
            fst.append(repr(struct.unpack('<d', opnd)[0]))
        elif k == 'fpush':
            fst.append(entry[1])
        elif k == 'fvar':
            fst.append(sym(w, entry[1]))
        elif k == 'fstore':
            emit(f'{sym(w, entry[1])} = {fpop()}')
        elif k == 'i2f':
            fst.append(entry[1].format(a=pop()))
        elif k == 'f2i':
            stack.append(entry[1].format(a=fpop()))
        elif k == 'fun':
            fst.append(entry[1].format(a=fpop()))
        elif k == 'fbin':
            bb = fpop(); a = fpop(); fst.append(f'({a} {entry[1]} {bb})')
        elif k == 'fcmp':
            bb = fpop(); a = fpop(); stack.append(f'({a} {entry[1]} {bb})')
        elif k == 'vcoerce':
            pop(); pop()
        elif k == 'pushcur':
            stack.append(f'{int.from_bytes(opnd, "little", signed=True)/10000}@')
        elif k == 'pushstr':
            n = int.from_bytes(opnd[2:4], 'little')
            s = opnd[6:6+n].decode('latin1').rstrip('"')
            stack.append('"' + s + '"')
        elif k == 'pushvar':
            stack.append(sym(w, entry[1]))
        elif k == 'pushaddr':
            stack.append(sym(w, entry[1]))
        elif k == 'pushref':
            stack.append('*' + sym(w, entry[1]))
        elif k == 'store':
            emit(f'{sym(w, entry[1])} = {pop()}')
        elif k == 'arr1':
            i = pop(); stack.append(f'{sym(w2, "G")}({i})')
        elif k == 'arrstore1':
            i = pop(); v = pop(); emit(f'{sym(w2, "G")}({i}) = {v}')
        elif k == 'randomize':
            emit(f'Randomize {fpop()}')
        elif k == 'arrload':
            i = pop(); stack.append(f'{sym(w2, "G")}({i})')
        elif k == 'arrstore':
            i = pop(); v = pop(); emit(f'{sym(w2, "G")}({i}) = {v}')
        elif k == 'arraddr':
            i = pop(); stack.append(f'{sym(w2, "G")}({i})')
        elif k == 'field':
            r = pop(); stack.append(f'{r}.{FIELDS.get(w, f"f{w:X}")}')
        elif k == 'fieldstore':
            r = pop(); v = pop(); emit(f'{r}.{FIELDS.get(w, f"f{w:X}")} = {v}')
        elif k == 'bin':
            bb = pop(); a = pop(); stack.append(f'({a} {entry[1]} {bb})')
        elif k == 'un':
            a = pop(); stack.append(entry[1].format(a=a))
        elif k == 'drop2':
            pop()
        elif k == 'jmp':
            emit(f'GoTo L{w:04x}'); flush(pos)
        elif k == 'reljmp':
            pass
        elif k == 'jcc':
            emit(f'If Not ({pop()}) Then GoTo L{w:04x}')
        elif k == 'jcc0':
            bb = pop(); a = pop(); emit(f'If ({a} Or {bb}) = 0 Then GoTo L{w:04x}')
        elif k == 'case':
            c = pop(); pop(); emit(f'Case ({c})   \' else L{w:04x}')
            case_at[w] = casev[0]
        elif k == 'caserange':
            hi = pop(); lo = pop(); v = pop(); stack.append(f'{lo} <= {v} <= {hi}')
        elif k == 'casejmp':
            casev[0] = pop(); case_at[w] = casev[0]; emit(f'Select Case {casev[0]}')
        elif k == 'textwidth':
            s = pop(); o = pop(); stack.append(f'{o}.TextWidth({s})')
        elif k == 'forstep':
            step = pop(); lim = pop(); start = pop(); var = pop()
            body = int.from_bytes(b[base+w2:base+w2+2], 'little')
            emit(f'For {var} = {start} To {lim} Step {step}   \' body L{body:04x}, exit L{w2+2:04x}')
        elif k == 'pushsgl':
            stack.append(repr(struct.unpack('<f', opnd[4:8])[0]) + '!')
        elif k == 'pushlng':
            stack.append(str(int.from_bytes(opnd, 'little', signed=True)) + '&')
        elif k == 'strtmp':
            stack.append(f'StrTmp({sym(w, "G")})')
        elif k == 'for':
            lim = pop(); start = pop(); var = pop()
            body = int.from_bytes(b[base+w2:base+w2+2], 'little')
            emit(f'For {var} = {start} To {lim}   \' body L{body:04x}, exit L{w2+2:04x}')
        elif k == 'next':
            emit(f'Next  \' loop slot {w:#x}')
        elif k == 'ret':
            emit('Exit/End Sub'); flush(pos)
        elif k == 'end':
            emit('End')
        elif k == 'onerror':
            emit(f'On Error GoTo {"0" if w == 0xffff else f"L{w:04x}"}')
        elif k == 'call':
            nargs = w; args = [pop() for _ in range(nargs)][::-1]
            nm = handle_name(w2)
            if nm in FUNCTIONS: stack.append(f'{nm}({", ".join(args)})')
            else: emit(f'Call {nm}({", ".join(args)})')
        elif k == 'callslot':
            nargs = w; args = [pop() for _ in range(nargs)][::-1]
            nm = sym(w2, 'D')
            if nm.startswith('D'): nm = 'Fn_' + nm
            stack.append(f'{nm}({", ".join(args)})')
        elif k == 'calldll':
            args = [pop() for _ in range(w)][::-1]
            emit(f'RtStmt{w2 & 0xfff}({", ".join(args)})')
        elif k == 'rtcall':
            fn = H.SEGS[2][(w2 & 0xff) >> 1]
            emit(f'RtCall fn={fn:#x} a={w}   \' stack={stack}'); stack.clear()
        elif k == 'rt14':
            a = pop(); stack.append(f'Rt14({a})')
        elif k == 'rtstack':
            emit(f'RtStack   \' stack={stack}'); stack.clear()
        elif k == 'propset':
            r = pop(); tag = pop(); v = pop(); emit(f'{r}.{sym(w, "P")} = {v}')
        elif k == 'propsetv':
            tag = pop(); v = pop(); emit(f'{sym(w, "G")}.<prop> = {v}')
        elif k == 'propget':
            r = pop(); stack.append(f'{r}.{sym(w, "P")}')
        elif k == 'propgetv':
            stack.append(f'{sym(w, "G")}.<prop>')
        elif k == 'propcall':
            i = pop(); stack.append(f'{sym(w2, "G")}({i})')
        elif k == 'tmpbuf':
            stack.append(f'TmpBuf({w})')
        elif k == 'setlen':
            pass
        else:
            emit(f'{k} {opnd.hex(" ")}   \' stack={stack}')
    for pr in problems:
        out.append(f'        \' PROBLEM {pr}')

def main():
    H.load(os.path.join(V.GAME, 'vbrun100.dll'))
    load_syms()
    mods = V.load_modules()
    want = sys.argv[1:] or ['RODENT2_FRM']
    for mname in want:
        b = mods[mname]
        procs = decode_module(mname, b)
        out = [f"' module {mname}"]
        for idx, p in enumerate(procs):
            pretty_proc(mname, b, idx, p, out)
        open(os.path.join(OUT, mname + '.vb'), 'w').write('\n'.join(out) + '\n')
        print(mname, len(procs), 'procs ->', mname + '.vb')
    with open(os.path.join(OUT, 'unknown.txt'), 'w') as f:
        for op, cnt in sorted(UNKNOWN.items()):
            oi = V.info(op)
            f.write(f'\n== {op:04x} count={cnt} size={oi.size} kind={oi.kind}\n')
            for l in V.handler_snippet(op, 16): f.write(l + '\n')

if __name__ == '__main__':
    main()
