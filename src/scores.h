/*
 * scores.h - high-score tables, one per level pack
 *
 * Stands in for WEPUTIL.DLL's WepScore / WepFame ("Hall of Fame"), which
 * kept Entertainment Pack scores in EntPack.dat.  Stored in
 * $XDG_DATA_HOME/rodentrecomp/scores.txt.
 */

#ifndef SCORES_H
#define SCORES_H

#include <stdbool.h>

#define SCORE_MAX   10
#define NAME_MAX_   12

typedef struct {
    char name[NAME_MAX_ + 1];
    long score;
    int  level;          /* level reached, 1-based */
    int  speed;          /* 0 Snail .. 4 Blazing */
    char date[11];       /* YYYY-MM-DD */
} Score;

typedef struct {
    Score s[SCORE_MAX];  /* best first */
    int   n;
} ScoreTable;

void scores_load(const char *pack, ScoreTable *t);
/* Where score would rank (0-based), or -1 if it doesn't make the table */
int  scores_rank(const ScoreTable *t, long score);
/* Adds e to pack's table and saves; returns its rank or -1 */
int  scores_add(const char *pack, const Score *e);
bool scores_clear(const char *pack);
const char *scores_path(void);

#endif /* SCORES_H */
