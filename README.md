# Open Revenge

An open-source port of **Rodent's Revenge**, the 1991 puzzle game by
Christopher Lee Fraley from Microsoft Entertainment Pack 2, for Linux
desktops and handhelds.

The game logic is transcribed from the original's Visual Basic 1.0 p-code,
so it plays like the original. It adds level packs, menus, gamepad support
and high scores. You need your own copy of the original game: its graphics
are read from `rodent.exe` at startup, and no Microsoft files are included
here.

## Playing

You are a mouse. Push blocks to trap the cats. When every cat on the board
is stuck, they all turn into cheese, which you can eat for points. Watch out
for the cats, and in later rooms for holes, mouse traps and yarn balls.

A level lasts 30 minutes on the stopwatch. More cats arrive every few
minutes. Trap everything after minute 25 and the clock runs out and you move
on.

### Keyboard

| Key | Action |
|---|---|
| Arrow keys | Move |
| Home, PgUp, End, PgDn | Move diagonally (NW, NE, SW, SE) |
| Numpad 1–9 | Move in 8 directions |
| Esc | Menu (pause) |
| F2 | New game |
| F3 | Pause (as in the original) |
| Enter, Backspace | Select and back in menus |

The original's hidden keys still work: **Ctrl+Shift+PgUp/PgDn** changes the
level, and **Ctrl+Shift+F10** ends the game.

### Gamepad

| Button | Action |
|---|---|
| D-pad or left stick | Move, including diagonals (press two directions together) |
| A | Select |
| B | Back |
| Start | Menu (pause) |

## Building

You need a C compiler, CMake, pkg-config, SDL2 and SDL2_image. On Debian or
Ubuntu:

```sh
sudo apt install build-essential cmake pkg-config libsdl2-dev libsdl2-image-dev
```

Then:

```sh
cmake -S . -B build
cmake --build build
```

## Running

Put `rodent.exe` from Microsoft Entertainment Pack 2 in one of these folders:

- the folder the `rodent` program is in
- the folder you run it from
- `./rodents_revenge`
- `~/.local/share/rodentrecomp`

Or pass `--game DIR`. Only `rodent.exe` is needed. If it isn't found, the
game says where it looked.

Then run it:

```sh
./build/rodent
```

The game finds its `data/` folder next to the program, one folder up (as
with `build/rodent`), in the folder you run it from, or in
`~/.local/share/rodentrecomp/data`. So a copy with `rodent`, `data/` and
`rodent.exe` together in one folder works from anywhere, which is how a
handheld port would ship it. `--data DIR` overrides the search.

The title screen has the level pack, starting level, high scores and
settings: speed, board size, colour or black-and-white, skin and fullscreen.

| File | Contents |
|---|---|
| `~/.config/rodentrecomp/settings.ini` | Settings, saved on every change |
| `~/.local/share/rodentrecomp/scores.txt` | High scores, one table per level pack |

### Command-line options

These override the saved settings for one run.

| Option | Effect |
|---|---|
| `--game DIR` | Folder with `rodent.exe` |
| `--pack FILE` | Level pack: a file in `data/packs` or a path |
| `--level N` | Starting level (1–50) |
| `--speed N` | 0 Snail, 1 Slow, 2 Medium, 3 Fast, 4 Blazing |
| `--large` | 16×16 tiles instead of 12×12 |
| `--mono` | Black and white, as on a monochrome display |
| `--fullscreen`, `--windowed` | Window mode |
| `--skin DIR` | Replacement graphics (see *Modding*) |
| `--data DIR` | Where `data/` is (normally found on its own) |
| `--dump-assets DIR` | Save the game's graphics as PNGs, then exit |

`--help` lists everything, including the testing options.

## Modding

### Level packs

Levels come from pack files in `data/packs/`. `original.pack` holds the six
rooms of the 1991 game. The original generates each room from a recipe, so
they come out different every time, just as they did then.

A pack can mix generated rooms with fixed 23×23 layouts drawn in text. It
can also choose how a level is won: the original clock rule, eat every
cheese, or trap every cat. The format is described in
[`data/packs/README.md`](data/packs/README.md).

`tools/lvl2pack.py` converts packs from the older Open Revenge `.lvl`
format.

### Skins

```sh
./build/rodent --dump-assets mygraphics
```

This writes the game's sprite sheets and stopwatch face as PNGs, with a
`README.txt` explaining the tile layout. To use edited copies, put them in
a folder under `data/skins/`, where they appear in Settings → Skin, or pass
`--skin DIR`. A skin only needs the images it changes; the rest come from
`rodent.exe`.

## How it was made

`rodent.exe` is a Visual Basic 1.0 program: its logic is p-code interpreted
by `VBRUN100.DLL`, not machine code. `tools/vbdis` decodes that p-code into
readable listings. `src/game.c` is a procedure-by-procedure transcription of
them, with comments quoting the original where the C is not obvious. The
field control `FIELD100.DLL` was reconstructed from its disassembly.
[`docs/GAME_LOGIC.md`](docs/GAME_LOGIC.md) documents the recovered rules,
tables and formulas.

What the original didn't have lives in its own files: level packs
(`pack.c`), menus (`menu.c`), settings, high scores (`scores.c`), and
gamepad input in `platform.c`. The original used `WEPUTIL.DLL`'s shared
high-score dialog instead. The few additions inside the game logic, such as
the extra level goals, are marked "not in the original" in `src/game.c`.

## Tests

```sh
SDL_VIDEODRIVER=dummy build/goal_test
SDL_VIDEODRIVER=dummy build/menu_test
```

Run them from the project folder. `rodent.exe` must be findable, as for the
game. `goal_test` covers the level goals. `menu_test` drives the menus,
settings, high scores and a virtual gamepad.

## Licence

The code is MIT-licensed (see [`LICENSE`](LICENSE)). Bundled third-party
files keep their own licences:

- DejaVu Sans: [`data/LICENSE-DejaVu.txt`](data/LICENSE-DejaVu.txt)
- stb_truetype and stb_image_write: public domain or MIT

Rodent's Revenge and its graphics are © Microsoft and are not part of this
repository.
