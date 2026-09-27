/*
 * game.h - Rodent's Revenge game logic
 *
 * Transcribed from the VB1 p-code of RODENT2.FRM (decoded with
 * tools/vbdis, see decompiled/rodent/vb_pcode/RODENT2_FRM.vb and
 * docs/GAME_LOGIC.md).  Names follow the original source where known.
 */

#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

/* ---- constants from the declarations section ---- */

#define cFieldC     23
#define cFieldCM1   (cFieldC - 1)
#define cFieldCM2   (cFieldC - 2)
#define cFieldCM3   (cFieldC - 3)
#define cFieldCD2   (cFieldC / 2)

enum {
    modeDEMO = 1, modeBEGINGAME, modeBEGINLEVEL, modeRESTART, modeBEGIN,
    modePLAY, modePLAY_FAST, modeYARN, modeDYING, modeEND, modeENDLEVEL,
    modeENDGAME,
    modeGRAB,           /* not in the original: goal: cats, time to eat the cheese */
    modeLAST
};

enum {
    chOPEN = 0, chBLOCK = 1, chWALL = 2, chVERT = 3, chHORZ = 4,
    chHOLE = 5, chTRAP = 6, chCHEESE = 7, chHOLEGUY = 8,
    chKAT = 0x10, chYARN = 0x11, chYARN_LAST = 0x14,
    chGUYGREY = 0x15, chGREY = 0x16, chKATSLEEP = 0x17,
    chGUY = 0x20, chDEAD = 0x25
};

enum { typeKAT = 0, typeYARN = 1 };

#define cbadMaxC    20

/* Type GUYTYPE */
typedef struct {
    int x, y;
    int ch;
    int type;     /* typeKAT / typeYARN */
    int kind;     /* yarn behaviour variant (0..4) */
} GUYTYPE;

typedef struct {
    int  mode;          /* modeG */
    int  lvl;           /* lvlG */
    const struct LevelDef *lv;   /* pack level for lvlG (was lvl6G = lvlG Mod 6) */
    bool fLevelCats;    /* grid cats not yet placed this level */
    int  lvlStart;      /* lvlStartG (0-based) */
    int  cMin, cSec;    /* game clock */
    int  cguy;          /* lives */
    int  timeBase;      /* mpitime(skill) */
    int  stun;
    long score;
    double subScore;    /* fractional score accumulator (Currency) */
    int  time;          /* timeG: current timer interval (ms) */
    int  modYarn;
    int  skill;
    bool fMono;
    int  ibadYarn;      /* index of the yarn ball in flight, 0 = none */
    int  rgdirYarn[9];
    bool mpdirokYarn[8];
    int  dirYarn;
    GUYTYPE guy[cbadMaxC + 1];   /* guyG(0..cbadMaxC); 0 is the mouse */
    int  ibadLast;
    bool fDoMelt;
    bool timerEnabled;  /* t.Enabled */
    bool paused;        /* t.Enabled cleared by F3 */
    bool gameOver;      /* "Game Over" label shown */
    int  grab;          /* modeGRAB ticks left */
} GameState;

extern GameState g;

extern const int mpdirdx[8];
extern const int mpdirdy[8];
extern const int mpitime[5];

struct Pack;
void game_init(int skill, int start_level, const struct Pack *levels, unsigned seed);  /* seed 0 = time */
void mode_set(int mode);
void timer_tick(void);
void key_down(int vk, int shift);
void new_game(void);
void pause_toggle(void);
void set_speed(int skill);
void hud_paint(bool labels);          /* labels: Game Over / Paused / demo text */
void game_set_start_level(int lvl);

#endif /* GAME_H */
