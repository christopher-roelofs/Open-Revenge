# vbdis — Visual Basic 1.0 p-code decoder for rodent.exe

`rodent.exe` is not native code: it is VB1 p-code interpreted by `vbrun100.dll`.
Ghidra therefore produced nothing useful for it.  These scripts recover the game
logic directly from the p-code.

## How it works

* `ne.py` — minimal NE (16-bit Windows) parser: segments, relocations, resources.
* `handlers.py` — each p-code opcode is a near offset into segment 2 of
  `vbrun100.dll` (threaded code: every handler ends with `es: lodsw; jmp ax`).
  `trace(op)` symbolically walks a handler (following near/far calls, the
  type-dispatch tables reached through `jmp bx`) and records how many operand
  bytes it consumes and whether it branches, so operand sizes never had to be
  guessed.
* `vbdis.py` — locates the four modules (RCDATA 2..5 = RODENT2.FRM, RODENT.GLB,
  LEVEL.FRM, PIX.FRM), splits them into procedures
  (`[locals][len][len2][18 00]` headers, 5-byte trailers), and decodes each
  procedure by following control flow.  Writes `out/*.lst` (raw listing) and
  `out/opcodes.txt` (every opcode seen, with its handler disassembly).
* `decomp.py` — pseudo-VB printer.  Opcode semantics (`OPS` table) were read off
  the handler disassembly; expressions are rebuilt with a symbolic integer stack
  plus a separate FPU stack.  Names come from `symbols.txt` (data addresses
  recovered from the declarations block: consts are initialised in name-table
  order, `Dim`s are the inline `373f <addr>` words, DLL/function handles from
  the global module).  Writes `out/*.vb`.

Procedure *k* of RODENT2.FRM is the *k*-th routine in the global module's
routine list (`AppSize, AppInit, MyRand, Form_Load, …, LblDo, Form_Unload`);
its call handle is `0x70 + 0x2E*(k-1)`.  Functions return through result
slots (`0x34 MyRand … 0x6C MinMod`) and are called with opcode `3d49`.

## Running

```
/usr/bin/python3 vbdis.py      # needs capstone (system python)
/usr/bin/python3 decomp.py RODENT2_FRM RODENT_GLB LEVEL_FRM PIX_FRM
```

Copies of the output live in `decompiled/rodent/vb_pcode/`.

## Known gaps

Only UI layout code is left undecoded (`TextWidth`, label placement in
`LblDo`, the clock face in `pixTime_Paint`).  Everything that affects gameplay
decodes without `??` markers.
