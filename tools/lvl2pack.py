#!/usr/bin/env python3
"""Convert an Open Revenge .lvl level pack to a .pack file (data/packs/README.md).

    tools/lvl2pack.py IN.lvl OUT.pack

.lvl packs start with "; Key: Value" header comments; each level is a
"; Level name" comment followed by grid rows.  Grids smaller than 23x23 are
centred and surrounded by wall.  The old game ended a level once every cat
was trapped and its cheese collected: levels with cheese get "goal: cheese",
other levels with cats "goal: cats" (trapping them all ends the level);
levels with cats get no batches or waves, and levels without cats keep
room 1's waves so they can still be played.  Yarn is off: the old game had none.
"""

import sys

SIZE = 23
TILES = set(".WBHTCMK")


def parse(path):
    header, levels = {}, []
    name, rows = None, []

    def flush():
        nonlocal rows
        if rows:
            levels.append((name or f"Level {len(levels) + 1}", rows))
        rows = []

    for raw in open(path, encoding="utf-8"):
        line = raw.rstrip("\r\n")
        text = line.strip()
        if text.startswith(";"):
            body = text[1:].strip()
            key, sep, value = body.partition(":")
            if not levels and not rows and sep and key.strip() in (
                    "Name", "Description", "Author", "Date", "Difficulty"):
                header[key.strip()] = value.strip()
                continue
            flush()
            name = body
        elif not text:
            flush()
        else:
            rows.append(text)
    flush()
    return header, levels


def fit(name, rows):
    h, w = len(rows), max(len(r) for r in rows)
    if h > SIZE or w > SIZE:
        sys.exit(f"{name}: {w}x{h} is larger than {SIZE}x{SIZE}")
    grid = [["W"] * SIZE for _ in range(SIZE)]
    y0, x0 = (SIZE - h) // 2, (SIZE - w) // 2
    for y, row in enumerate(rows):
        for x, c in enumerate(row.ljust(w, "W")):
            if c not in TILES:
                sys.exit(f"{name}: unknown tile {c!r}")
            grid[y0 + y][x0 + x] = c
    for i in range(SIZE):
        for x, y in ((i, 0), (i, SIZE - 1), (0, i), (SIZE - 1, i)):
            if grid[y][x] != "W":
                print(f"warning: {name}: {grid[y][x]} on the outer ring replaced by W", file=sys.stderr)
                grid[y][x] = "W"
    return ["".join(r) for r in grid]


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    header, levels = parse(sys.argv[1])
    out = [f"; Converted from {sys.argv[1].split('/')[-1]} by tools/lvl2pack.py", ""]
    for key, pkey in (("Name", "name"), ("Author", "author"), ("Date", "year"), ("Description", "description")):
        if key in header:
            value = header[key][:4] if pkey == "year" else header[key]
            out.append(f"{pkey}: {value.replace(';', ',')}")
    for name, rows in levels:
        grid = fit(name, rows)
        cats = sum(r.count("K") for r in grid)
        cheese = sum(r.count("C") for r in grid)
        out += ["", "[level]", f"name: {name.replace(';', ',')}"]
        if cheese:
            out.append("goal: cheese")
        elif cats:
            out.append("goal: cats")
        if cats:
            out += ["batch: 0", "wave: 0", "interval: 30"]
        else:
            out += ["batch: 3", "wave: 3", "interval: 5"]
        out += ["yarn: off", "grid:"] + grid
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")
    print(f"{sys.argv[2]}: {len(levels)} levels")


if __name__ == "__main__":
    main()
