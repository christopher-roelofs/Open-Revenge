/*
 * pack.h - level packs
 *
 * A pack is a text file of "key: value" lines: pack-wide keys first, then
 * one [level] section per level.  Levels are played in order and repeat
 * after the last one (the original's lvlG Mod 6).  A level is either
 * generated (LevelDraw's patterns) or a fixed grid.  See data/packs/README.md.
 */

#ifndef PACK_H
#define PACK_H

#include <stdbool.h>
#include <stdint.h>
#include "game.h"

enum {
    patSQUARE,          /* solid square of blocks, walls scattered (lvl6 0, 1) */
    patCHECKER,         /* checkerboard of blocks, walls scattered (lvl6 3) */
    patSCATTER,         /* walls and blocks scattered (lvl6 2, 4) */
    patCHECKER_WALLS,   /* checkerboard of walls, blocks scattered (lvl6 5) */
    patGRID             /* fixed layout from the pack */
};

enum {
    goalCLOCK,          /* original: trap every cat after minute 25 */
    goalCHEESE,         /* eat every cheese placed in the grid */
    goalCATS            /* trap every cat on the board, then a moment to eat */
};

typedef struct LevelDef {
    char name[64];
    int  pattern;
    int  blocks;        /* mplvlpery: % of obstacles that are pushable blocks */
    int  difficulty;    /* added to the level number in the density formulas (was lvl6) */
    int  batch;         /* mplvlck: cats (in halves) whenever a batch starts */
    int  wave;          /* mplvldk: cats (in halves) added at each wave */
    int  interval;      /* mplvlmin: clock minutes between waves */
    bool yarn;          /* yarn balls (from level 3 on, as in the original) */
    int  goal;          /* goalCLOCK / goalCHEESE */
    /* patGRID only */
    uint8_t grid[cFieldC][cFieldC];   /* [y][x], ch* values */
    int  startX, startY;              /* mouse start */
    int  ccat;                        /* cats placed at the level's first batch */
    int  catX[cbadMaxC], catY[cbadMaxC];
} LevelDef;

typedef struct Pack {
    char name[64];
    char author[64];
    char year[16];
    char description[160];
    int  clevel;
    LevelDef *levels;
} Pack;

/* Returns false and prints the reason (file:line) on error. */
bool pack_load(Pack *pack, const char *path);
void pack_free(Pack *pack);

#endif /* PACK_H */
