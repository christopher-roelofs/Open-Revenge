#!/usr/bin/env python3
"""Builds data/packs/mouse-academy.pack and checks each grid.

    tools/mouse_academy.py data/packs/mouse-academy.pack
"""
import sys
from collections import deque

N = 23


def canvas():
    g = [['.'] * N for _ in range(N)]
    for i in range(N):
        g[0][i] = g[N - 1][i] = g[i][0] = g[i][N - 1] = 'W'
    return g


def rect(g, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            g[y][x] = c


def put(g, cells, c):
    for x, y in cells:
        g[y][x] = c


def mirror4(cells):
    """(x, y) plus its reflections across the centre lines"""
    out = set()
    for x, y in cells:
        for a in (x, N - 1 - x):
            for b in (y, N - 1 - y):
                out.add((a, b))
    return sorted(out)


# ---- the levels ----

def first_squeak():
    g = canvas()
    rect(g, 8, 8, 14, 14, 'B')
    put(g, [(11, 11)], 'M')
    put(g, [(19, 3)], 'K')
    return g


def cheese_trail():
    g = canvas()
    for y, gap in ((5, 'right'), (9, 'left'), (13, 'right'), (17, 'left')):
        rect(g, 1, y, 21, y, 'B')
        if gap == 'right':
            rect(g, 19, y, 21, y, '.')
        else:
            rect(g, 1, y, 3, y, '.')
    for y in (3, 7, 11, 15, 19):
        put(g, [(6, y), (16, y)], 'C')
    put(g, [(11, 3)], 'M')
    put(g, [(11, 20)], 'K')
    return g


def double_trouble():
    g = canvas()
    rect(g, 3, 9, 7, 13, 'B')
    rect(g, 15, 9, 19, 13, 'B')
    rect(g, 11, 6, 11, 9, 'B')
    rect(g, 11, 13, 11, 16, 'B')
    put(g, mirror4([(4, 4)]) + [(11, 3), (11, 19)], 'W')
    put(g, [(11, 11)], 'M')
    put(g, [(2, 2), (20, 20)], 'K')
    return g


def sinkholes():
    g = canvas()
    rect(g, 8, 8, 14, 14, 'B')
    for y in range(N):
        for x in range(N):
            if max(abs(x - 11), abs(y - 11)) == 6 and (x + y) % 3 == 0:
                g[y][x] = 'H'
    put(g, mirror4([(3, 3)]), 'H')
    put(g, [(11, 11)], 'M')
    put(g, [(2, 2), (20, 2), (11, 20)], 'K')
    return g


def minefield():
    g = canvas()
    for y in range(3, 20):
        for x in range(3, 20):
            if (x * 3 + y * 7) % 11 == 0:
                g[y][x] = 'T'
    rect(g, 9, 9, 13, 13, '.')            # a safe square in the middle
    rect(g, 10, 10, 12, 12, 'B')
    put(g, mirror4([(5, 2), (2, 7)]), 'B')
    cheese = [(4, 4), (18, 4), (4, 18), (18, 18), (11, 5), (11, 17), (5, 11), (17, 11)]
    for x, y in cheese:                   # cheese never sits on a trap
        g[y][x] = 'C'
    put(g, [(11, 9)], 'M')
    put(g, [(2, 20), (20, 2)], 'K')
    return g


def rush_hour():
    g = canvas()
    for y in range(2, 21):
        for x in range(2, 21):
            if x % 3 != 0 and y % 3 != 0:
                g[y][x] = 'B'
    put(g, [(12, 12)], 'M')
    return g


def crossfire():
    g = canvas()
    for cx, cy in mirror4([(5, 5)]) + [(11, 11)]:
        rect(g, cx - 1, cy - 1, cx + 1, cy + 1, 'B')
        g[cy][cx] = '.'
    rect(g, 8, 3, 14, 3, 'B')
    rect(g, 8, 19, 14, 19, 'B')
    rect(g, 3, 8, 3, 14, 'B')
    rect(g, 19, 8, 19, 14, 'B')
    put(g, [(11, 11)], 'M')
    put(g, [(2, 11), (20, 11)], 'K')
    return g


def the_vault():
    g = canvas()
    # four walled vaults, each with a block door facing the middle
    for x0, y0, door, cheese in (
            (2, 2, (6, 4), [(3, 3), (4, 5)]),
            (16, 2, (16, 4), [(19, 3), (18, 5)]),
            (2, 16, (6, 18), [(3, 19), (4, 17)]),
            (16, 16, (16, 18), [(19, 19), (18, 17)])):
        rect(g, x0, y0, x0 + 4, y0 + 4, 'W')
        rect(g, x0 + 1, y0 + 1, x0 + 3, y0 + 3, '.')
        put(g, [door], 'B')
        put(g, cheese, 'C')
    rect(g, 9, 9, 13, 13, 'B')
    rect(g, 10, 2, 12, 3, 'B')
    rect(g, 10, 19, 12, 20, 'B')
    put(g, [(11, 11)], 'M')
    put(g, [(11, 6), (2, 11), (20, 11)], 'K')
    return g


def revenge():
    g = canvas()
    rect(g, 7, 7, 15, 15, 'B')
    for y in range(N):
        for x in range(N):
            if max(abs(x - 11), abs(y - 11)) == 7 and (x + y) % 4 == 0:
                g[y][x] = 'H'
    # corner cheese, guarded by traps on two sides
    for cx, cy in mirror4([(2, 2)]):
        g[cy][cx] = 'C'
        sx = 1 if cx < 11 else -1
        sy = 1 if cy < 11 else -1
        g[cy + sy][cx] = 'T'
        g[cy][cx + sx] = 'T'
    put(g, [(11, 11)], 'M')
    put(g, [(3, 11), (19, 11)], 'K')
    return g


LEVELS = [
    ("First Squeak", first_squeak,
     "Push blocks to pin the cat. Walls count as one side of the trap.",
     dict(goal="cats", batch=0, wave=0, interval=30, yarn="off")),
    ("Cheese Trail", cheese_trail,
     "Eat all ten cheeses. Cats erase cheese they walk over, and a pushed block crushes it.",
     dict(goal="cheese", batch=0, wave=2, interval=4, yarn="off")),
    ("Double Trouble", double_trouble,
     "Two cats: they only turn to cheese when both are stuck at the same moment.",
     dict(goal="cats", batch=0, wave=0, interval=30, yarn="off")),
    ("Sinkholes", sinkholes,
     "Cats can't cross holes, so use them as trap walls. Blocks pushed in vanish, and you get stuck.",
     dict(goal="cats", batch=0, wave=0, interval=30, yarn="off")),
    ("Minefield", minefield,
     "Traps stop cats and blocks but kill you. The cheese hides among them.",
     dict(goal="cheese", batch=0, wave=2, interval=5, yarn="off")),
    ("Rush Hour", rush_hour,
     "Two cats, and more every two minutes. Trap everything on the board to get out.",
     dict(goal="cats", batch=4, wave=3, interval=2, yarn="off")),
    ("Crossfire", crossfire,
     "Yarn balls roll in from the edges when you line up with them. Blocks are cover.",
     dict(goal="cats", batch=0, wave=2, interval=5, yarn="on")),
    ("The Vault", the_vault,
     "Open each vault by pushing its door in, without crushing the cheese. The cats will follow you in.",
     dict(goal="cheese", batch=0, wave=3, interval=3, yarn="off")),
    ("Checkmate", None,
     "A fresh checkerboard every time, with holes, traps and yarn.",
     dict(pattern="checker", blocks=100, difficulty=5, goal="cats", batch=6, wave=4, interval=3, yarn="on")),
    ("Revenge", revenge,
     "Everything at once, under the original rule: survive, and trap them all after minute 25.",
     dict(goal="clock", batch=4, wave=4, interval=4, yarn="on", difficulty=3)),
]


# ---- checks ----

def check(name, g):
    errs = []
    cells = {c: [(x, y) for y in range(N) for x in range(N) if g[y][x] == c] for c in "MKCHTBW."}
    if len(cells['M']) != 1:
        errs.append("needs exactly one M")
    for i in range(N):
        for x, y in ((i, 0), (i, N - 1), (0, i), (N - 1, i)):
            if g[y][x] != 'W':
                errs.append(f"border not wall at {(x, y)}")
    # mouse reach: walls and traps stop it; blocks can be pushed (approximation)
    if cells['M']:
        seen, q = {cells['M'][0]}, deque(cells['M'])
        while q:
            x, y = q.popleft()
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    p = (x + dx, y + dy)
                    if p not in seen and g[p[1]][p[0]] not in "WT":
                        seen.add(p)
                        q.append(p)
        for c in cells['C']:
            if c not in seen:
                errs.append(f"cheese {c} unreachable")
    # a cat that starts with no open neighbour would be trapped at once
    for x, y in cells['K']:
        if not any(g[y + dy][x + dx] in ".MC" for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy):
            errs.append(f"cat {(x, y)} starts stuck")
    return errs, {c: len(v) for c, v in cells.items()}


def main():
    out = [
        "; Mouse Academy: one new idea per level, then all of them together.",
        "; Generated by tools/mouse_academy.py: edit that and rerun it.",
        "",
        "name: Mouse Academy",
        "author: Open Revenge",
        "year: 2026",
        "description: Ten lessons in mousery, from one cat to everything at once.",
    ]
    bad = False
    for name, build, desc, keys in LEVELS:
        out += ["", "[level]", f"name: {name}", f"; {desc}"]
        out += [f"{k}: {v}" for k, v in keys.items()]
        if build:
            g = build()
            errs, counts = check(name, g)
            summary = ", ".join(f"{c}={n}" for c, n in counts.items() if c in "KCHTB" and n)
            print(f"{name:15s} {summary}")
            for e in errs:
                print(f"   ERROR: {e}")
                bad = True
            out.append("grid:")
            out += ["".join(r) for r in g]
        else:
            print(f"{name:15s} generated")
    with open(sys.argv[1], "w") as f:
        f.write("\n".join(out) + "\n")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
