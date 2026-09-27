# Rodent's Revenge — game logic recovered from the VB1 p-code

Source: `decompiled/rodent/vb_pcode/RODENT2_FRM.vb` (decoded with `tools/vbdis`).
Everything below is what the original binary does, not a reconstruction.

## Field and tiles

* Field is **23×23** (`cFieldC = 23`), cells 0..22; border walls on row/col 0 and 22.
  `field100.dll` stores the grid as `tile[(x << shift) + y]` and blits sprite
  column = `id & 0xF`, row = `id >> 4` (low nibble is the column).
* Tile ids: `chOPEN=0 chBLOCK=1 chWALL=2 chVERT=3 chHORZ=4 chHOLE=5 chTRAP=6
  chCHEESE=7 chHOLEGUY=8 chKAT=&H10 chYARN=&H11 … chYARN_LAST=&H14
  chGUYGREY=&H15 chGREY=&H16 chKATSLEEP=&H17 chGUY=&H20 … chDEAD=&H25`
  (`&H21..&H25` are the mouse death frames, `&H12..&H14` yarn fade frames).
* `field100` calls used: `FldErase, FldDraw(ctl,x,y,ch), FldDrawOn, FldGet, FldMelt`
  only. `FldDrawWalls/FldSmoothWalls/FldDirGet` are never used.

## Entities

`guyG(0..20) As GUYTYPE {x, y, ch, type, dir}`; index 0 is the mouse,
1..`ibadLastG` are cats and yarn balls (`cbadMaxC = 20`). `BadKill(i)` moves
the last entry into slot *i*.

## Tables (all per `lvl6G = lvlG Mod 6`)

| | 0 | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|---|
| `mplvlmin` (minutes between cat waves) | 5 | 4 | 5 | 2 | 3 | 4 |
| `mplvlck` (cats at level start, ÷2) | 3 | 3 | 6 | 2 | 4 | 4 |
| `mplvldk` (cats per wave, ÷2) | 3 | 3 | 4 | 2 | 3 | 3 |
| `mplvlpery` (block %) | 100 | 90 | 87 | 100 | 50 | 50 |

* `mpitime` (Snail…Blazing) = 2000, 1000, 500, 275, 180 ms.
* `mpmodetime`: DEMO 500; BEGINGAME, BEGINLEVEL, BEGIN, DYING, ENDLEVEL, ENDGAME 100;
  PLAY_FAST, YARN 50. PLAY uses `SetTimer`.
* Directions 0..7 = N, NE, E, SE, S, SW, W, NW (`mpdirdx = 0,1,1,1,0,-1,-1,-1`,
  `mpdirdy = -1,-1,0,1,1,1,0,-1`).
* Keys: numpad 1-9 → `mpkeydir = 5,4,3,6,-1,2,7,0,1`; PgUp,PgDn,End,Home,←,↑,→,↓
  → `mpkeydir2 = 1,3,5,7,6,0,2,4`. Esc pauses. Ctrl+Shift+PgUp/PgDn changes level,
  Ctrl+Shift+F10 sets `fDoMelt` (ends the game with the melt effect).

## Level generation (`LevelDraw`)

```
perY = mplvlpery(lvl6)          ' pushable blocks
perB = 100 - perY               ' stationary walls
perH = 0: If lvl >= 3 Then perH = Min(15, 2*(lvl+lvl6-4)): perY = Max(0, perY-perH)
perT = 0: If lvl >= 6 Then perT = Min(10, lvl+lvl6-5):      perB = Max(0, perB-perT)
If perY < perB-15 Then t = perB+perY: perY = t\2 - 7: perB = perY + 15
border of chWALL
cBlock = 200 - 5*lvl - 5*lvl6: If cBlock*perY < 7000 Then cBlock = 7000 \ perY
Select Case lvl6
  0,1: solid square of chBLOCK, side Sqr(cBlock), centred on (11,11)
  3:   checkerboard of chBLOCK ((x+y) And 1), side Sqr(2*cBlock), centred
  5:   checkerboard of chWALL ((x+y) And 1 = 1), side Sqr(2*perB*cBlock/100)
  2,4: cBlock*perB/100 chWALL at random interior cells (1 + MyRand(20))
cBlock*perH/100 chHOLE and cBlock*perT/100 chTRAP at random interior cells
lvl6 = 2,4,5: cBlock*perY/100 + 1 chBLOCK at random interior cells
```
`MyRand(n) = CInt(n * Rnd)` clamped to `n-1`. Mouse starts at (11,11)
(`GuyNew`); on restart it is re-placed if any bad guy is within distance² < tries.

## Mode machine (`t_timer` / `ModeSet`)

DEMO → (F2) BEGINGAME → BEGINLEVEL → BEGIN → PLAY ⇄ PLAY_FAST/YARN → DYING →
RESTART or ENDGAME; ENDLEVEL → BEGINLEVEL.

* BEGINGAME: lives `cguyG = 3`, score 0, `lvlG = lvlStartG - 1`.
* BEGINLEVEL: `LevelNew(-1)` (lvl+1), timer reset, `ibadLastG = 0`, `GuyNew`.
* BEGIN tick: `BadNew(mplvlck(lvl6), typeKAT)`; if `lvl > 3` also `BadNew(2, typeYARN)`; → PLAY.
* PLAY tick: `BadMove`, `TimeAdd 1`, `ScoreAdd(66.67 / timeG * (1 + 3*(cMinG=30)))`.
* PLAY_FAST (all cats stuck): `BadMove`, `TimeAdd 5` (seconds snapped to 5);
  at `cMinG >= 25` `ScoreAdd(0.67)` per tick; `cMinG >= 30` → ENDLEVEL; at a wave boundary
  (`cSecG = 0 And MinMod() = 0`) → END → BEGIN (new cats arrive).
* ENDLEVEL: `ScoreAdd(100 * (lvl+1))` → BEGINLEVEL.
* DYING tick: mouse `ch` counts up to `chDEAD` then `GuyKill` (`cguyG -= 1`;
  0 → ENDGAME else RESTART). RESTART: `cSecG = 0`, `cMinG -= MinMod()`, `GuyNew(-1)`.
* Time: `TimeAdd d`: `cSecG += d`; every 30 sec → `cMinG += 1` (cap 30).
  Tick interval `timeG = CInt(mpitime(skill) * 0.98^lvl * 0.945^(cMinG \ mplvlmin(lvl6)))`.
  `modYarn = 2*timeG \ 50`.

## Mouse (`GuyMove dir`)

Blocked while `stunG <> 0`. Target cell `c = FldGet(x+dx, y+dy)`:
* `chOPEN` → move. `chCHEESE` → `ScoreAdd(100 + 25*lvl)`, move.
* `chBLOCK` → `FPushBlock(chBLOCK, x, y, dx, dy)`; if it returns True, move.
* `chTRAP` → move, `ModeSet modeDYING`. `chHOLE` → `stunG = 10`, draw `chHOLEGUY`.
* Anything else (wall, cat, yarn): nothing.

`FPushBlock(ch, x, y, dx, dy)` walks along the push direction:
`chOPEN` → draw `ch` there, True. `chHOLE` → True, block disappears (a pushed cat
is drawn instead). `chCHEESE` → overwritten, True. `chKAT` → if the cat can step
aside (`BadFMoveKat`) re-check the cell, otherwise push the cat recursively.
`chBLOCK` → keep walking (whole row moves). Wall/trap/anything else → False.

## Cats (`BadMove`, every PLAY tick)

1. `stunG` counts down; at 0 the mouse is redrawn.
2. At the top of a minute (`cSecG = 0`), if `cMinG <> 0 And cMinG Mod mplvlmin(lvl6) = 0
   And cMinG <= 25`: `BadNew(mplvldk(lvl6), typeKAT)`.
3. If `lvl > 2 And Rnd < 1 - 0.99^lvl`: `BadNew(1, typeYARN)`.
4. Each bad guy: yarn → `BadMoveYarn`; cat → `BadFMoveKat` (count the ones that moved).
5. If **no** cat moved: switch to PLAY_FAST and turn every cat into `chCHEESE`
   (`BadKill`). Cats are "trapped" collectively, not by a 1×1 test.

`BadNew(n, type)` accumulates `n` in `cbadPending(type)` and spawns
`cbadPending \ 2` entities (max `cbadMaxC`, yarn max `cbadMaxC-3`). Cats appear
on random `chOPEN` cells at least √100 away from the mouse; yarn starts on a
border cell disguised as `chWALL`.

`BadFMoveKat(i)`: 10 % chance to skip the greedy step. Greedy: `dx = Sgn(mouse.x - x)`,
`dy = Sgn(mouse.y - y)`; try diagonal, then (50 % each order) vertical/horizontal.
Fallback: up to 8 random open directions that still reduce distance on one axis.
Then all 8 directions in a shuffled order. No move at all → `chKATSLEEP`, returns False.
A cat that steps onto the mouse's cell (`BadFOpen` allows `chGUY`, `chHOLEGUY`,
`chOPEN`, `chCHEESE`) triggers `modeDYING`.

## Yarn (`BadMoveYarn`, `YarnDirFFire`, `YarnMove`)

A yarn ball sits on the border as `chWALL`; with 10 %/tick it becomes `chHORZ`/`chVERT`
(edge tint), then `chYARN`. Once visible, `YarnDirFFire` (per `dir` field 1/2/3 and
random 5-10 % chances) fires when the mouse is within one row/column or on a diagonal;
`ModeSet modeYARN` and the ball travels one cell per 50 ms tick (`YarnMove`):
`chGUY`/`chHOLEGUY` → mouse dies; `chWALL`/`chBLOCK`/edge → bounce back, fade
through `chYARN+1..chYARN_LAST` and, with probability `Min(0.9, 0.05*lvl + 0.05*lvl6)`
on a free cell, return to PLAY; when the fade finishes the ball is removed
(`BadKill`) and PLAY resumes. Other cats still move every `modYarn` yarn ticks.

## Scoring

* Per PLAY tick: `66.67 / timeG` points; per YARN-mode cat step `200 / timeG`
  (both become negative once the clock reaches 30 minutes).
* Cheese: `100 + 25*lvl`. Level clear: `100 * (lvl+1)`.
* `ScoreAdd` keeps a currency fraction accumulator; score is clamped at 0.
* Settings in `entpack.ini` [Rodent]: `Size`, `Skill` (0-3, default 1), `Room` (start level 0-49).
