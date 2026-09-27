# Level packs

A pack is one text file. Load it with `rodent --pack FILE`. The default is
`data/packs/original.pack`, the six rooms of the 1991 game.

Levels play in order and repeat after the last one: a six-level pack's
level 7 is its first level again. The overall level number keeps counting,
so each pass is faster and denser than the one before.

## Format

`key: value` lines. `;` starts a comment. Pack keys come first, then one
`[level]` section per level.

```ini
name: My Pack
author: Someone
year: 2026
description: One line about the pack.

[level]
name: Warm-up
pattern: square
blocks: 100

[level]
name: The Box
batch: 0
wave: 2
interval: 5
grid:
WWWWWWWWWWWWWWWWWWWWWWW
W.....................W
...  (23 rows of 23)
```

### Level keys

| Key | Default | Meaning |
|---|---|---|
| `name` | none | Shown on the level's intro card. |
| `hint` | none | One or two sentences under the name on the intro card: what's new, or how to win. No `;` (it starts a comment). |
| `pattern` | — | A generated room: `square`, `checker`, `scatter` or `checker-walls`. |
| `grid:` | — | A fixed room. The 23 rows follow on the next lines. |
| `blocks` | 100 | Generated rooms: % of obstacles that are pushable blocks rather than walls. |
| `difficulty` | 0 | Generated rooms: added to the level number when working out how many blocks, holes, traps and yarn behaviours appear. The original rooms use 0–5. |
| `batch` | 3 | Cats at the start of each batch, **in half-cats**. |
| `wave` | 3 | Cats added at each wave during play, **in half-cats**. |
| `interval` | 5 | Clock minutes between waves (1–30). |
| `yarn` | `on` | Yarn balls, from level 3 on as in the original. |
| `goal` | `clock` | `clock`: the original rule. `cheese`: eat every `C` in the grid. `cats`: trap every cat. See *Ending a level*. |

Each level needs exactly one of `pattern` or `grid`.

### How cats and the clock work

The clock has 30 minutes of 30 ticks each.

- **Batches:** `batch` cats appear when the level begins.
- **Waves:** every `interval` minutes up to minute 25, `wave` more cats join.
- **Half-cats:** counts are in half-cats and the remainder carries over. A
  `batch` of 3 gives 1 cat, then 2, then 1.
- **Trapping:** when every cat is stuck at once they all turn into cheese.
  The clock then fast-forwards to the next wave time, where a new batch
  arrives. After minute 25 there are no more waves, so it runs on to 30.
- **Death:** losing a life rewinds the clock to the start of the current
  wave interval.

`docs/GAME_LOGIC.md` has the details.

### Ending a level

With `goal: clock` (the default, and the original rule), the level ends when
the clock reaches 30, and it only gets there by fast-forwarding. So the level
really ends when every cat is trapped after minute 25. If the clock reaches
30 with cats still loose, the score drains each tick until they're trapped.

With `goal: cats` (not in the original), trapping every cat on the board
ends the level instead of fast-forwarding the clock. The cats turn into
cheese as usual, and you get up to 5 seconds to eat it (the HUD counts down
"Clear!"); the level ends as soon as no cheese is left, or when time runs
out. Waves still arrive while cats are loose, so trap them fast. For
Gopher's Grievance-style levels where cats never come back, use `batch: 0`
and `wave: 0` with the cats as `K` in the grid. The level needs cats at the
start: a `batch` above 0 or a `K`.

With `goal: cheese` (not in the original, fixed grids only), eating the
last cheese placed in the grid (`C`) ends the level straight away, cats or
not. Cheese from trapped cats is a bonus and doesn't count. The clock rule
still runs, but when a trap fast-forwards to 30 the level waits there until
the grid's cheese is gone. If cats or blocks destroy the last of it, the
level ends as with `goal: clock`. Don't seal cheese behind walls: the level
can't end until it's eaten.

### Generated patterns

| Pattern | Room |
|---|---|
| `square` | A solid square of blocks in the middle, walls scattered. |
| `checker` | A checkerboard of blocks, walls scattered. |
| `scatter` | Walls and blocks scattered at random. |
| `checker-walls` | A checkerboard of walls, blocks scattered. |

Generated rooms can get holes from level 4 and traps from level 7, depending on `difficulty`.

### Grid tiles

The grid is 23×23 and its outer ring must be walls.

| Tile | Meaning |
|---|---|
| `.` | open floor |
| `W` | wall |
| `B` | pushable block |
| `H` | hole: the mouse is stuck for 10 ticks; blocks pushed in vanish |
| `T` | mouse trap: stepping on it costs a life |
| `C` | cheese |
| `M` | mouse start (default: the centre) |
| `K` | a cat, present from the start in addition to `batch` |

## Converting Open Revenge packs

`tools/lvl2pack.py OLD.lvl NEW.pack` converts `.lvl` packs. Smaller grids are
centred and padded with wall. Levels with cheese get `goal: cheese`, other
levels with cats `goal: cats`, and levels without cats get room 1's waves.
