/*
 * game.c - Rodent's Revenge game logic
 *
 * Direct transcription of the VB1 p-code of RODENT2.FRM
 * (decompiled/rodent/vb_pcode/RODENT2_FRM.vb).  Each function below
 * corresponds to one VB procedure of the same name; comments quote the
 * original where the C is not obvious.
 */

#include "game.h"
#include "pack.h"
#include "field.h"
#include "platform.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

GameState g;

/* ---- tables initialised in AppInit ---- */

const int mpdirdx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };   /* 0=N clockwise */
const int mpdirdy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
static const int mpkeydir[9]  = { 5, 4, 3, 6, -1, 2, 7, 0, 1 };   /* numpad 1..9 */
static const int mpkeydir2[8] = { 1, 3, 5, 7, 6, 0, 2, 4 };       /* PgUp..Down (VK 33..40) */
const int mpitime[5] = { 2000, 1000, 500, 275, 180 };            /* Snail..Blazing */
static const int mptypech[2] = { chKAT, chYARN };
/* mplvlmin/mplvlck/mplvldk/mplvlpery (indexed by lvl6) now come from the
 * level pack: LevelDef interval/batch/wave/blocks, lvl6 -> difficulty */
static const Pack *pack;
static int mpmodetime[modeLAST + 1];

/* ---- helpers standing in for VB runtime / controls ---- */

static double Rnd(void)
{
    return rand() / ((double)RAND_MAX + 1.0);
}

static int Sgn(int v) { return (v > 0) - (v < 0); }

/* FldDraw fld, x, y, ch */
static void FldDraw(int x, int y, int ch) { field_draw((uint8_t)ch, x, y); }
static int  FldGet(int x, int y)          { return field_get(x, y); }

static void refresh_hud(void);

/* ---- forward declarations (VB order) ---- */

static int  MyRand(int max);
static void LevelNew(int lvl);
static void LevelDraw(void);
static void GuyKill(void);
static void GuyNew(bool fRand);
static void GuyPaint(void);
static void GuyMove(int dir);
static int  BadIFindBad(int x, int y);
static bool FPushBlock(int ch, int x, int y, int dx, int dy);
static bool BadFOpen(int x, int y);
static void BadKill(int ibad);
static void BadNew(int cbad, int type);
static void BadPlace(const LevelDef *lv);
static bool FCheeseLeft(void);
static void BadMove(void);
static bool BadFMoveKat(int ibad);
static void BadMoveYarn(int ibad);
static void YarnMove(void);
static int  YarnFillDir(int ibad, bool fDiag);
static int  YarnDirFFire(int ibad);
static void TimeReset(void);
static void TimeAdd(int dSec);
static void ScoreReset(void);
static void ScoreAdd(double dscore);
static void SetTimer(void);
static int  MinMod(void);
static void LblDo(int op);

/* ---------------------------------------------------------------- */

/* Function MyRand (max As Integer) As Integer */
static int MyRand(int max)
{
    int fRand = (int)floor(max * Rnd() + 0.5);   /* CInt(max * Rnd(1)) */
    if (fRand >= max) fRand = max - 1;
    return fRand;
}

/* Sub AppInit */
static void AppInit(unsigned seed)
{
    g.fMono = platform_mono();                 /* 2 = GetDeviceCaps(hdc, NUMCOLORS) */
    srand(seed ? seed : (unsigned)platform_ticks());   /* Randomize Timer */
    memset(mpmodetime, 0, sizeof(mpmodetime));
    mpmodetime[modeDEMO] = 500;
    mpmodetime[modeBEGINGAME] = 100;
    mpmodetime[modeBEGINLEVEL] = 100;
    mpmodetime[modeBEGIN] = 100;
    mpmodetime[modePLAY_FAST] = 50;
    mpmodetime[modeYARN] = 50;
    mpmodetime[modeDYING] = 100;
    mpmodetime[modeENDLEVEL] = 100;
    mpmodetime[modeENDGAME] = 100;
    mpmodetime[modeGRAB] = 100;
}

/* Sub uSpeed_Click (Index As Integer) */
void set_speed(int skill)
{
    if (skill < 0) skill = 0;
    if (skill > 4) skill = 4;
    g.timeBase = mpitime[skill];
    g.skill = skill;
    if (g.mode == modePLAY) SetTimer();
}

/* Sub Form_Load */
void game_init(int skill, int start_level, const Pack *levels, unsigned seed)
{
    memset(&g, 0, sizeof(g));
    pack = levels;
    field_init(cFieldC, cFieldC);
    AppInit(seed);
    LblDo(0);
    if (skill < 0 || skill > 3) skill = 1;      /* GetPrivateProfileInt("Skill", 1) */
    set_speed(skill);
    if (start_level < 0 || start_level > 49) start_level = 0;
    g.lvlStart = start_level;
    g.lvl = g.lvlStart;
    mode_set(modeDEMO);
}

/* Not in the original: the title menu's starting level; redraws the demo
 * room like Ctrl+Shift+PgUp/PgDn in fld_KeyDown */
void game_set_start_level(int lvl)
{
    g.lvlStart = lvl < 0 ? 0 : lvl;
    if (g.mode == modeDEMO) {
        LevelNew(g.lvlStart);
        refresh_hud();
    }
}

/* Sub uNew_Click */
void new_game(void)
{
    /* the original asks "Ok to start new game?" outside demo mode */
    mode_set(modeBEGINGAME);
}

/* Sub uPause_Click: t.Enabled = Not t.Enabled : LblDo 1 + (1 = t.Enabled) */
void pause_toggle(void)
{
    if (g.mode == modeDEMO) return;
    g.paused = !g.paused;
    LblDo(g.paused ? 2 : 1);
}

/* Sub fld_KeyDown (KeyCode, Shift) */
void key_down(int vk, int shift)
{
    if (vk == 0x1B) {                         /* Esc: WindowState = 1 (minimise) */
        return;
    }
    if ((shift & 3) == 3) {                   /* Ctrl+Shift cheats */
        if (vk == 0x79) {                     /* F10 */
            g.fDoMelt = true;
        } else if (vk == 0x22 || vk == 0x21) {  /* PgDn / PgUp */
            if (vk == 0x22) g.lvlStart++;
            else { g.lvlStart--; if (g.lvlStart < 0) g.lvlStart = 0; }
            if (g.mode == modeDEMO) {
                LevelNew(g.lvlStart);
                refresh_hud();
            } else {
                mode_set(modeBEGINLEVEL);
            }
        }
    }
    if (g.paused || g.mode == modeDYING || g.mode == modeDEMO)
        return;
    if (vk >= 33 && vk <= 40)
        GuyMove(mpkeydir2[vk - 33]);
    else if (vk >= 97 && vk <= 105)
        GuyMove(mpkeydir[vk - 97]);
}

/* Sub LevelNew (lvl As Integer) */
static void LevelNew(int lvl)
{
    field_erase(chOPEN);
    if (lvl >= 0) g.lvl = lvl;
    else          g.lvl = g.lvl + 1;
    g.lv = &pack->levels[g.lvl % pack->clevel];   /* lvl6G = lvlG Mod 6 */
    g.fLevelCats = true;
    refresh_hud();                            /* Caption = szAppCaptionC & Format$(lvlG + 1, " [##]") */
    LevelDraw();
}

/* Sub LevelDraw */
static void LevelDraw(void)
{
    const LevelDef *lv = g.lv;
    int perY = lv->blocks;
    int perB = 100 - perY;
    int perH = 0, perT = 0;
    int i, x, y;
    long cBlock;

    if (lv->pattern == patGRID) {
        for (y = 0; y < cFieldC; y++)
            for (x = 0; x < cFieldC; x++)
                if (lv->grid[y][x] != chOPEN) FldDraw(x, y, lv->grid[y][x]);
        return;
    }
    if (g.lvl >= 3) {
        perH = 2 * ((g.lvl + lv->difficulty) - 4);
        if (perH > 15) perH = 15;
        perY -= perH;
        if (perY < 0) perY = 0;
    }
    if (g.lvl >= 6) {
        perT = (g.lvl + lv->difficulty) - 5;
        if (perT > 10) perT = 10;
        perB -= perT;
        if (perB < 0) perB = 0;
    }
    if (perY < perB - 15) {
        i = perB + perY;
        perY = (i / 2) - 7;
        perB = perY + 15;
    }
    for (i = 0; i <= cFieldCM1; i++) {
        FldDraw(i, 0, chWALL);
        FldDraw(i, cFieldCM1, chWALL);
        FldDraw(0, i, chWALL);
        FldDraw(cFieldCM1, i, chWALL);
    }
    cBlock = (200 - 5 * g.lvl) - 5 * lv->difficulty;
    if (perY > 0 && cBlock * perY < 7000) cBlock = 7000 / perY;

    switch (lv->pattern) {                    /* Select Case lvl6 */
    case patSQUARE: {                         /* 0, 1 */
        int h = (int)(sqrt((double)cBlock)) / 2;
        for (x = cFieldCD2 - h; x <= cFieldCD2 + h; x++)
            for (y = cFieldCD2 - h; y <= cFieldCD2 + h; y++)
                FldDraw(x, y, chBLOCK);
        break;
    }
    case patCHECKER: {                        /* 3 */
        int h = (int)(sqrt((double)(2 * cBlock))) / 2;
        for (x = cFieldCD2 - h; x <= cFieldCD2 + h; x++)
            for (y = cFieldCD2 - h; y <= cFieldCD2 + h; y++)
                if ((x + y) & 1) FldDraw(x, y, chBLOCK);
        break;
    }
    default:
        break;
    }
    if (lv->pattern == patCHECKER_WALLS) {    /* lvl6 = 5 */
        int h = (int)(sqrt((double)((2L * perB * cBlock) / 100))) / 2;
        for (x = cFieldCD2 - h; x <= cFieldCD2 + h; x++)
            for (y = cFieldCD2 - h; y <= cFieldCD2 + h; y++)
                if (((x + y) & 1) == 1) FldDraw(x, y, chWALL);
    } else {
        for (i = 1; i <= (int)((cBlock * perB) / 100); i++)
            FldDraw(1 + MyRand(cFieldCM3), 1 + MyRand(cFieldCM3), chWALL);
    }
    for (i = 1; i <= (int)((cBlock * perH) / 100); i++)
        FldDraw(1 + MyRand(cFieldCM3), 1 + MyRand(cFieldCM3), chHOLE);
    for (i = 1; i <= (int)((cBlock * perT) / 100); i++)
        FldDraw(1 + MyRand(cFieldCM3), 1 + MyRand(cFieldCM3), chTRAP);
    switch (lv->pattern) {
    case patSCATTER: case patCHECKER_WALLS:   /* 2, 4, 5 */
        for (i = 0; i <= (int)((cBlock * perY) / 100); i++)
            FldDraw(1 + MyRand(cFieldCM3), 1 + MyRand(cFieldCM3), chBLOCK);
        break;
    }
}

/* Sub GuyKill */
static void GuyKill(void)
{
    g.cguy--;
    GuyPaint();
    if (g.cguy <= 0) mode_set(modeENDGAME);
    else             mode_set(modeRESTART);
}

/* Sub GuyNew (fRand As Integer) */
static void GuyNew(bool fRand)
{
    int cTry = 50;
    int x = g.lv->startX;                     /* cFieldCD2 unless the grid has an M */
    int y = g.lv->startY;
    int i;

    g.guy[0].ch = chGUY;
    if (fRand) {
        for (i = 1; i <= g.ibadLast; i++) {
            double dx = g.guy[i].x - x, dy = g.guy[i].y - y;
            if (dx * dx + dy * dy < cTry) {
                x = MyRand(cFieldCM2) + 1;
                y = MyRand(cFieldCM2) + 1;
                cTry--;
                i = 0;                        /* GoTo start of the For loop */
                continue;
            }
            FldDraw(g.guy[i].x, g.guy[i].y, g.guy[i].ch);
        }
    }
    FldDraw(x, y, chGUY);
    g.guy[0].x = x;
    g.guy[0].y = y;
}

/* Sub GuyPaint: draws the remaining lives on pixGuy */
static void GuyPaint(void)
{
    /* pixGuy is repainted every frame by hud_paint() */
}

/* Sub GuyMove (dir As Integer) */
static void GuyMove(int dir)
{
    int x, y, ch, dx, dy;
    bool fAte = false;

    if (dir == -1 || g.stun) return;
    x = g.guy[0].x + mpdirdx[dir];
    y = g.guy[0].y + mpdirdy[dir];
    ch = FldGet(x, y);
    switch (ch) {
    case chOPEN:
    move:
        FldDraw(g.guy[0].x, g.guy[0].y, chOPEN);
        g.guy[0].x = x;
        g.guy[0].y = y;
        FldDraw(x, y, chGUY);
        break;
    case chBLOCK:
        dx = mpdirdx[dir];
        dy = mpdirdy[dir];
        if (FPushBlock(chBLOCK, x, y, dx, dy)) {
            FldDraw(g.guy[0].x, g.guy[0].y, chOPEN);
            g.guy[0].x += dx;
            g.guy[0].y += dy;
            goto move;
        }
        break;
    case chTRAP:
        FldDraw(g.guy[0].x, g.guy[0].y, chOPEN);
        g.guy[0].x = x;
        g.guy[0].y = y;
        FldDraw(x, y, chGUY);
        mode_set(modeDYING);
        break;
    case chHOLE:
        g.stun = 10;
        FldDraw(g.guy[0].x, g.guy[0].y, chOPEN);
        g.guy[0].x = x;
        g.guy[0].y = y;
        FldDraw(x, y, chHOLEGUY);
        break;
    case chCHEESE:
        ScoreAdd(100 + 25 * g.lvl);
        fAte = true;
        goto move;
    }
    /* goal: cheese (not in the original): the grid's last cheese ends the level */
    if (fAte && g.lv->goal == goalCHEESE && !FCheeseLeft() &&
        g.mode != modeBEGINGAME && g.mode != modeBEGINLEVEL && g.mode != modeENDLEVEL)
        mode_set(modeENDLEVEL);
}

/* Function BadIFindBad (x, y) As Integer */
static int BadIFindBad(int x, int y)
{
    for (int i = 1; i <= g.ibadLast; i++)
        if (g.guy[i].x == x && g.guy[i].y == y)
            return i;
    return 0;
}

/* Function FPushBlock (ch, x, y, dx, dy) As Integer */
static bool FPushBlock(int ch, int x, int y, int dx, int dy)
{
    int c, ibad;
    bool f;

    for (;;) {
        x += dx;
        y += dy;
    recheck:
        c = FldGet(x, y);
        switch (c) {
        case chOPEN:
            FldDraw(x, y, ch);
            return true;
        case chHOLE:
            if (ch != chBLOCK) FldDraw(x, y, ch);   /* blocks vanish into holes */
            return true;
        case chCHEESE:
            FldDraw(x, y, ch);
            return true;
        case chKAT:
            ibad = BadIFindBad(x, y);
            if (ibad == 0) return false;
            if (!BadFMoveKat(ibad)) goto recheck;  /* cat stepped aside: look again */
            f = FPushBlock(chKAT, x, y, dx, dy);
            if (f) {
                FldDraw(x, y, ch);
                g.guy[ibad].x += dx;
                g.guy[ibad].y += dy;
            }
            return f;
        case chBLOCK:
            if (ch != chBLOCK) {
                f = FPushBlock(chBLOCK, x, y, dx, dy);
                if (f) FldDraw(x, y, ch);
                return f;
            }
            continue;                              /* whole row moves */
        default:
            return false;
        }
    }
}

/* Function BadFOpen (x, y) As Integer */
static bool BadFOpen(int x, int y)
{
    int c = FldGet(x, y);
    return c == chGUY || c == chHOLEGUY || c == chOPEN || c == chCHEESE;
}

/* Sub BadKill (ibad As Integer) */
static void BadKill(int ibad)
{
    g.guy[ibad] = g.guy[g.ibadLast];
    g.ibadLast--;
}

/* Sub BadNew (cbad As Integer, type As Integer) */
static void BadNew(int cbad, int type)
{
    static int cbadPending[2];
    int i;

    cbadPending[type] += cbad;
    if (cbadPending[type] < 2) return;
    cbad = cbadPending[type] / 2;
    cbadPending[type] -= 2 * cbad;
    if (cbad + g.ibadLast > cbadMaxC) return;

    switch (type) {
    case typeKAT:
        for (i = 1 + g.ibadLast; i <= cbad + g.ibadLast; i++) {
            GUYTYPE *b = &g.guy[i];
            for (;;) {
                double dx, dy;
                b->x = MyRand(cFieldC);
                if (b->x >= cFieldC) b->x = cFieldCM1;
                b->y = MyRand(cFieldC);
                if (b->y >= cFieldC) b->y = cFieldCM1;
                if (FldGet(b->x, b->y) != chOPEN) continue;
                dx = b->x - g.guy[0].x; dy = b->y - g.guy[0].y;
                if (i != 0 && dx * dx + dy * dy < 100) continue;
                break;
            }
            b->type = type;
            b->ch = mptypech[type];
            FldDraw(b->x, b->y, mptypech[type]);
        }
        break;
    case typeYARN:
        if (g.ibadLast + cbad > cbadMaxC - 3) return;
        for (i = 1 + g.ibadLast; i <= cbad + g.ibadLast; i++) {
            GUYTYPE *b = &g.guy[i];
            int kmax, x, y;
            b->type = type;
            b->ch = chWALL;
            kmax = g.lvl - 2;
            if (kmax > 5) kmax = 5;
            b->kind = kmax > 0 ? MyRand(kmax) : 0;
            x = MyRand(cFieldC);
            y = x;
            if (Rnd() < 0.25)           x = 0;
            else if (Rnd() < 0.333333)  x = cFieldCM1;
            else if (Rnd() < 0.5)       y = 0;
            else                        y = cFieldCM1;
            b->x = x;
            b->y = y;
        }
        break;
    }
    g.ibadLast += cbad;
}

/* Not in the original: the cats (K) of a fixed-grid level */
static void BadPlace(const LevelDef *lv)
{
    int i;
    for (i = 0; i < lv->ccat && g.ibadLast < cbadMaxC; i++) {
        GUYTYPE *b;
        if (FldGet(lv->catX[i], lv->catY[i]) != chOPEN) continue;
        b = &g.guy[++g.ibadLast];
        b->x = lv->catX[i];
        b->y = lv->catY[i];
        b->type = typeKAT;
        b->ch = chKAT;
        FldDraw(b->x, b->y, chKAT);
    }
}

/* Not in the original: any cheese at all on the board (modeGRAB)? */
static bool FAnyCheese(void)
{
    int x, y;
    for (y = 0; y < cFieldC; y++)
        for (x = 0; x < cFieldC; x++)
            if (FldGet(x, y) == chCHEESE) return true;
    return false;
}

/* Not in the original: is any of the grid's own cheese left (goal: cheese)?
 * Cheese from trapped cats is a bonus and does not count. */
static bool FCheeseLeft(void)
{
    int x, y;
    for (y = 0; y < cFieldC; y++)
        for (x = 0; x < cFieldC; x++)
            if (g.lv->grid[y][x] == chCHEESE && FldGet(x, y) == chCHEESE) return true;
    return false;
}

/* Sub BadMove */
static void BadMove(void)
{
    int i, cCantMove = 0, ch;

    if (g.stun != 0) {
        g.stun--;
        if (g.stun == 0)
            FldDraw(g.guy[0].x, g.guy[0].y, chGUY);
    }
    if (g.cSec == 0) {
        if (g.cMin != 0 && MinMod() == 0 && g.cMin <= 25)
            BadNew(g.lv->wave, typeKAT);
    }
    if (g.lv->yarn && g.lvl > 2 && Rnd() < 1.0 - pow(0.99, g.lvl))
        BadNew(1, typeYARN);

    for (i = 1; i <= g.ibadLast; i++) {
        if (g.guy[i].type == typeYARN) {
            cCantMove++;
            BadMoveYarn(i);
            continue;
        }
        if (g.mode == modeYARN) {
            ch = FldGet(g.guy[i].x, g.guy[i].y);
            if (ch >= chYARN && ch <= chYARN_LAST)
                continue;                 /* the ball is passing over this cat */
        }
        if (BadFMoveKat(i))
            cCantMove++;
    }
    if (cCantMove == g.ibadLast) {
        /* goal: cats (not in the original) ends the level here instead of
         * fast-forwarding the clock to the next batch */
        int modeNew = g.lv->goal == goalCATS ? modeGRAB : modePLAY_FAST;
        if (g.mode != modeNew)
            mode_set(modeNew);
        for (i = 1; i <= g.ibadLast; i++) {
            if (g.guy[i].type == typeKAT) {
                FldDraw(g.guy[i].x, g.guy[i].y, chCHEESE);
                BadKill(i);
            }
        }
    }
}

/*
 * Function BadFMoveKat (ibad As Integer) As Integer
 * Returns True when the cat could NOT move (it goes to sleep).
 */
static bool BadFMoveKat(int ibad)
{
    static bool fInited;
    static int dxL[9], dyL[9];
    GUYTYPE *b = &g.guy[ibad];
    int x = b->x, y = b->y;
    int dx, dy, i, cdir, r1, r2;

    if (Rnd() < 0.1) goto random_dir;

    dx = Sgn(g.guy[0].x - b->x);
    dy = Sgn(g.guy[0].y - b->y);
    if (BadFOpen(x + dx, y + dy)) { x += dx; y += dy; goto moved; }
    if (Rnd() < 0.5 && BadFOpen(x, y + dy)) { y += dy; goto moved; }
    if (BadFOpen(x + dx, y))   { x += dx; goto moved; }
    if (BadFOpen(x, y + dy))   { y += dy; goto moved; }

random_dir:
    cdir = YarnFillDir(ibad, true);
    if (cdir) {
        for (i = 1; i <= 8; i++) {
            int r = MyRand(cdir);
            dx = mpdirdx[g.rgdirYarn[r]];
            dy = mpdirdy[g.rgdirYarn[r]];
            if (BadFOpen(x + dx, y + dy) &&
                ((dx != 0 && dx == Sgn(g.guy[0].x - b->x)) ||
                 (dy != 0 && dy == Sgn(g.guy[0].y - b->y)))) {
                x += dx; y += dy; goto moved;
            }
        }
    }
    if (!fInited) {
        i = 0;
        for (dx = -1; dx <= 1; dx++)
            for (dy = -1; dy <= 1; dy++)
                if (dx != 0 || dy != 0) { dxL[i] = dx; dyL[i] = dy; i++; }
        fInited = true;
    }
    r1 = MyRand(8); r2 = MyRand(8);
    dx = dxL[r1]; dy = dyL[r1];
    dxL[r1] = dxL[r2]; dyL[r1] = dyL[r2];
    dxL[r2] = dx; dyL[r2] = dy;
    for (i = 0; i <= 8; i++) {
        if (BadFOpen(x + dxL[i], y + dyL[i])) {
            x += dxL[i]; y += dyL[i]; goto moved;
        }
    }
    FldDraw(b->x, b->y, chKATSLEEP);
    return true;

moved:
    FldDraw(b->x, b->y, chOPEN);
    b->x = x;
    b->y = y;
    if (x == g.guy[0].x && y == g.guy[0].y) {
        mode_set(modeDYING);
        return false;
    }
    FldDraw(x, y, chKAT);
    return false;
}

/* Sub BadMoveYarn (ibad As Integer) */
static void BadMoveYarn(int ibad)
{
    GUYTYPE *b = &g.guy[ibad];
    int dir;

    switch (b->ch) {
    case chWALL:
        if (Rnd() < 0.1) {
            if (b->x == 0 || b->x == cFieldCM1) b->ch = chHORZ;
            else                                b->ch = chVERT;
        }
        break;
    case chHORZ:
    case chVERT:
        b->ch = chYARN;
        break;
    case chYARN:
        dir = YarnDirFFire(ibad);
        if (dir != -1) {
            g.dirYarn = dir;
            mode_set(modeYARN);
        }
        break;
    }
    FldDraw(b->x, b->y, b->ch);
}

/* Sub YarnMove */
static void YarnMove(void)
{
    static int ibad, dxYarn, dyYarn, chOld;
    static bool fInited;
    int ch2, i;
    double per;
    GUYTYPE *b;

    ibad = g.ibadYarn;
    b = &g.guy[ibad];
    if (!fInited) {
        dxYarn = mpdirdx[g.dirYarn];
        dyYarn = mpdirdy[g.dirYarn];
        if (b->x == 0 || b->y == 0 || b->x == cFieldCM1 || b->y == cFieldCM1)
            chOld = chWALL;
        else
            chOld = chOPEN;
        fInited = true;
    }
    if (b->ch > chYARN && b->ch <= chYARN_LAST) {       /* fading out */
        if (b->ch == chYARN_LAST) {
            chOld = chOPEN;
            if (b->x == 0 || b->y == 0 || b->x == cFieldCM1 || b->y == cFieldCM1)
                chOld = chWALL;
            FldDraw(b->x, b->y, chOld);
            BadKill(ibad);
            g.mode = modePLAY;
            SetTimer();
            fInited = false;
            g.ibadYarn = 0;
            for (i = 1; i <= g.ibadLast; i++)
                FldDraw(g.guy[i].x, g.guy[i].y, g.guy[i].ch);
        } else {
            b->ch++;
            FldDraw(b->x, b->y, b->ch);
        }
        return;
    }
    FldDraw(b->x, b->y, chOld);
    b->x += dxYarn;
    b->y += dyYarn;
    chOld = FldGet(b->x, b->y);
    if (b->x <= 0 || b->y <= 0 || b->x >= cFieldCM1 || b->y >= cFieldCM1)
        chOld = chWALL;
    switch (chOld) {
    case chBLOCK:
    case chWALL:
        b->x -= dxYarn;
        b->y -= dyYarn;
        ch2 = FldGet(b->x, b->y);
        FldDraw(b->x, b->y, chYARN);
        per = 0.05 * g.lvl + 0.05 * g.lv->difficulty;
        if (per > 0.9) per = 0.9;
        if (Rnd() < per && (ch2 == chOPEN || ch2 == chHOLE || ch2 == chTRAP)) {
            g.mode = modePLAY;             /* ball stays put, play resumes */
            SetTimer();
            fInited = false;
            g.ibadYarn = 0;
            return;
        }
        b->ch = chYARN + 1;                /* start fading */
        break;
    case chGUY:
    case chHOLEGUY:
        fInited = false;
        chOld = chOPEN;
        b->ch = chOPEN;
        mode_set(modeDYING);
        BadKill(ibad);
        return;
    }
    FldDraw(b->x, b->y, b->ch);
}

/* Function YarnFillDir (ibad As Integer, fDiag As Integer) As Integer */
static int YarnFillDir(int ibad, bool fDiag)
{
    int x = g.guy[ibad].x, y = g.guy[ibad].y;
    int n = 0, dir, xT, yT, ch;
    int step = fDiag ? 1 : 2;              /* Step 2 + fDiag (fDiag = -1 when True) */

    for (dir = 0; dir <= 7; dir++) g.mpdirokYarn[dir] = false;
    for (dir = 0; dir <= 7; dir += step) {
        xT = x + mpdirdx[dir];
        yT = y + mpdirdy[dir];
        if (xT > 0 && xT < cFieldCM1 && yT > 0 && yT < cFieldCM1) {
            ch = FldGet(xT, yT);
            if (ch != chBLOCK && ch != chWALL) {
                g.rgdirYarn[n++] = dir;
                g.mpdirokYarn[dir] = true;
            }
        }
    }
    g.rgdirYarn[n] = -1;
    return n;
}

/* Function YarnDirFFire (ibad As Integer) As Integer: direction or -1 */
static int YarnDirFFire(int ibad)
{
    GUYTYPE *b = &g.guy[ibad];
    const GUYTYPE *m = &g.guy[0];
    int cdir, i, dir;

    if (g.ibadYarn != 0) return -1;
    if (Rnd() < 0.075) goto fire_random;

    switch (b->kind) {
    case 1:
    near_check:
        if (abs(b->y - m->y) <= 1 || abs(b->x - m->x) <= 1) {
        fire_random:
            cdir = YarnFillDir(ibad, g.lvl > 3);
            if (cdir) {
                dir = g.rgdirYarn[MyRand(cdir)];
                goto fire;
            }
        }
        return -1;
    case 2:
    axis_check:
        if (Rnd() < 0.1) goto near_check;
        if (b->x == 0 || b->x == cFieldCM1) {
            if (abs(b->y - m->y) <= 1) goto fire_random;
        } else {
            if (abs(b->x - m->x) <= 1) goto fire_random;
        }
        return -1;
    case 3:
    aimed:
        if (Rnd() < 0.1) goto axis_check;
        if (b->y == m->y) {
            YarnFillDir(ibad, true);
            if (b->x < m->x) { if (g.mpdirokYarn[2]) { dir = 2; goto fire; } }
            else             { if (g.mpdirokYarn[6]) { dir = 6; goto fire; } }
        }
        if (b->x == m->x) {
            YarnFillDir(ibad, true);
            if (b->y < m->y) { if (g.mpdirokYarn[4]) { dir = 4; goto fire; } }
            else             { if (g.mpdirokYarn[0]) { dir = 0; goto fire; } }
        }
        if (b->x - m->x == b->y - m->y) {
            YarnFillDir(ibad, true);
            if (b->y < m->y) { if (g.mpdirokYarn[3]) { dir = 3; goto fire; } }
            else             { if (g.mpdirokYarn[7]) { dir = 7; goto fire; } }
        }
        if (b->x - m->x == m->y - b->y) {
            YarnFillDir(ibad, true);
            if (b->x > m->x) { if (g.mpdirokYarn[5]) { dir = 5; goto fire; } }
            else             { if (g.mpdirokYarn[1]) { dir = 1; goto fire; } }
        }
        return -1;
    default:
        if (Rnd() < 0.05) goto axis_check;
        if (Rnd() < 0.9)  goto aimed;
        cdir = YarnFillDir(ibad, true);
        if (cdir) {
            for (i = 1; i <= 8; i++) {
                int d = g.rgdirYarn[MyRand(cdir)];
                if (mpdirdx[d] == Sgn(m->x - b->x) || mpdirdy[d] == Sgn(m->y - b->y)) {
                    dir = d;
                    goto fire;
                }
            }
        }
        return -1;
    }
fire:
    g.ibadYarn = ibad;
    return dir;
}

/* Sub TimeReset */
static void TimeReset(void)
{
    g.cMin = 0;
    g.cSec = 0;
    refresh_hud();
}

/* Sub TimeAdd (dSec As Integer) */
static void TimeAdd(int dSec)
{
    g.cSec += dSec;
    if (g.cSec >= 30) {
        g.cMin += g.cSec / 30;
        if (g.cMin >= 30) g.cMin = 30;
        g.cSec %= 30;
    }
    refresh_hud();
}

/* Sub ScoreReset */
static void ScoreReset(void)
{
    g.score = 0;
    refresh_hud();
}

/* Sub ScoreAdd (dscore As Currency) */
static void ScoreAdd(double dscore)
{
    g.subScore += dscore;
    g.score += (long)floor(g.subScore);
    if (g.score < 0) g.score = 0;
    g.subScore -= floor(g.subScore);
    refresh_hud();
}

/* Sub t_timer */
void timer_tick(void)
{
    if (!g.timerEnabled || g.paused) return;
    g.timerEnabled = false;
    switch (g.mode) {
    case modeBEGINGAME:
        mode_set(modeBEGINLEVEL);
        break;
    case modeBEGINLEVEL:
        mode_set(modeBEGIN);
        break;
    case modeRESTART:
        TimeAdd(1);
        mode_set(modePLAY);
        break;
    case modeBEGIN:
        if (g.fLevelCats) {                   /* cats placed in a fixed grid */
            BadPlace(g.lv);
            g.fLevelCats = false;
        }
        BadNew(g.lv->batch, typeKAT);
        if (g.lv->yarn && g.lvl > 3) BadNew(2, typeYARN);
        mode_set(modePLAY);
        break;
    case modePLAY:
        g.ibadYarn = 0;
        BadMove();
        TimeAdd(1);
        ScoreAdd(66.67 / g.time * (1 + 3 * (g.cMin == 30 ? -1 : 0)));
        if (g.fDoMelt) mode_set(modeENDGAME);
        break;
    case modePLAY_FAST: {
        bool fHold;
        BadMove();
        if (g.cSec % 5 != 0) g.cSec = (g.cSec / 5) * 5;
        TimeAdd(5);
        /* goal: cheese holds the clock at 30 until the cheese is eaten */
        fHold = g.cMin >= 30 && g.lv->goal == goalCHEESE && FCheeseLeft();
        if (g.cMin >= 25 && !fHold) ScoreAdd(0.67);
        if (g.cMin >= 30 && !fHold) mode_set(modeENDLEVEL);
        if (g.cMin <= 25 && g.cSec == 0 && MinMod() == 0) mode_set(modeEND);
        break;
    }
    case modeYARN: {
        static int cTick;
        YarnMove();
        cTick = (cTick + 1) % (g.modYarn > 0 ? g.modYarn : 1);
        if (cTick == 0) {
            BadMove();
            TimeAdd(1);
            ScoreAdd(200.0 / g.time * (1 + 2 * (g.cMin == 30 ? -1 : 0)));
        }
        break;
    }
    case modeDYING:
        if (g.guy[0].ch < chDEAD) {
            g.guy[0].ch++;
            FldDraw(g.guy[0].x, g.guy[0].y, g.guy[0].ch);
        } else {
            FldDraw(g.guy[0].x, g.guy[0].y, chOPEN);
            GuyKill();
        }
        break;
    case modeEND:
        TimeAdd(1);
        mode_set(modeBEGIN);
        break;
    case modeGRAB:                            /* goal: cats: eat what you can, then on */
        if (--g.grab <= 0 || !FAnyCheese())
            mode_set(modeENDLEVEL);
        break;
    case modeENDLEVEL:
        ScoreAdd(100 * (g.lvl + 1));
        mode_set(modeBEGINLEVEL);
        break;
    case modeENDGAME:
        mode_set(modeDEMO);
        break;
    }
    g.timerEnabled = true;
}

/* Sub ModeSet (modeNew As Integer) */
void mode_set(int modeNew)
{
    g.timerEnabled = false;
    switch (modeNew) {
    case modeDEMO:
        LblDo(0);
        TimeReset();
        g.cguy = 3;
        GuyPaint();
        LevelNew(g.lvlStart);
        g.timerEnabled = true;
        g.ibadYarn = 0;
        break;
    case modeBEGINGAME:
        LblDo(0);
        g.cguy = 3;
        GuyPaint();
        ScoreReset();
        g.lvl = g.lvlStart - 1;
        g.timerEnabled = true;
        break;
    case modeBEGINLEVEL:
        LevelNew(-1);
        TimeReset();
        g.ibadLast = 0;
        GuyNew(false);
        g.ibadYarn = 0;
        break;
    case modeRESTART:
        g.cSec = 0;
        g.cMin -= MinMod();
        refresh_hud();
        GuyNew(true);
        g.ibadYarn = 0;
        g.stun = 0;
        break;
    case modeGRAB:
        g.grab = 5000 / mpmodetime[modeGRAB];     /* Gopher's Grievance gave 5 s */
        break;
    case modePLAY:
        SetTimer();
        break;
    case modeENDGAME:
        g.fDoMelt = false;
        LblDo(1);
        field_melt();
        /* WepScore hwnd, szAppExeC, scoreG: high-score table */
        break;
    }
    g.mode = modeNew;
    if (mpmodetime[modeNew] != 0)
        g.time = mpmodetime[modeNew];
    g.timerEnabled = true;
}

/* Sub SetTimer */
static void SetTimer(void)
{
    g.time = (int)floor(g.timeBase * pow(0.98, g.lvl)
                        * pow(0.945, g.cMin / g.lv->interval) + 0.5);
    g.modYarn = (2 * g.time) / mpmodetime[modeYARN];
}

/* Function MinMod () As Integer */
static int MinMod(void)
{
    return g.cMin % g.lv->interval;
}

/* Sub LblDo (op): 0 hide, 1 "Game Over", 2 "Paused / Press F3 To Continue" */
static void LblDo(int op)
{
    g.gameOver = (op == 1);
    refresh_hud();
}

/* Caption: "Rodent's Revenge [n]" */
static void refresh_hud(void)
{
    char title[64];
    snprintf(title, sizeof title, "Rodent's Revenge [%d]", g.lvl + 1);
    platform_set_title(title);
}

/* pixTime_Paint: clock face - hour hand = level (12h dial), minute hand =
 * game minutes (30 = full turn), second hand = cSecG (30 = full turn) */
static void draw_clock(int cx, int cy, int r)
{
    double sec = g.cSec / 30.0;
    double min = (g.cMin + sec / 4.0) / 30.0;
    if (min > 1.0) min = 1.0;
    double hour = ((g.lvl + 1) + min / 2.0) / 12.0;
    double aH = 2 * M_PI * hour - M_PI / 2, aM = 2 * M_PI * min - M_PI / 2, aS = 2 * M_PI * sec - M_PI / 2;

    platform_draw_image("timer", cx - r, cy - r, 2 * r, 2 * r);
    platform_draw_line(cx, cy, cx + (int)(r * 0.5 * cos(aH)), cy + (int)(r * 0.5 * sin(aH)), 0, 0, 0);
    platform_draw_line(cx, cy, cx + (int)(r * 0.75 * cos(aM)), cy + (int)(r * 0.75 * sin(aM)), 0, 0, 0);
    platform_draw_line(cx, cy, cx + (int)(r * 0.85 * cos(aS)), cy + (int)(r * 0.85 * sin(aS)), 200, 0, 0);
}

/* Everything the VB form drew outside the field, in the strip above it as
 * on the original form: pixGuy (lives) at the left, pixTime centred, the
 * score (Form_Paint) at the right; lbl/lbl2 messages over the field. */
void hud_paint(bool labels)
{
    int hx, hy, hw, hh, fx, fy, fw, fh;
    int tw = platform_tile_width();
    char buf[64];

    platform_hud_rect(&hx, &hy, &hw, &hh);
    platform_field_rect(&fx, &fy, &fw, &fh);

    /* lives: GuyPaint draws min(cguy-1, 5) mice then grey blocks on pixGuy;
     * level number below it (the original only showed it in the caption,
     * which a fullscreen handheld never displays) */
    {
        int iLast = g.cguy - 1, i;
        if (iLast > 5) iLast = 5;
        for (i = 1; i <= 5; i++)
            platform_draw_tile_px(i <= iLast ? chGUYGREY : chGREY, hx + (i - 1) * tw, hy);
        if (g.mode == modeGRAB)               /* seconds left to eat the cheese */
            snprintf(buf, sizeof buf, "Clear! %d", (g.grab * mpmodetime[modeGRAB] + 999) / 1000);
        else
            snprintf(buf, sizeof buf, "Level %d", g.lvl + 1);
        platform_draw_text(FONT_NORMAL, buf, hx, hy + hh - platform_text_height(FONT_NORMAL), 0x000000);
    }

    /* clock */
    {
        int r = hh / 2;
        draw_clock(hx + hw / 2, hy + r, r);
    }

    /* score: Form_Paint prints it straight onto the form, right-aligned */
    {
        int th = platform_text_height(FONT_BIG);
        char digits[32];
        int n, i, o = 0;
        n = snprintf(digits, sizeof digits, "%ld", g.score < 0 ? -g.score : g.score);
        if (g.score < 0) buf[o++] = '-';
        for (i = 0; i < n; i++) {                  /* Format$(scoreG, "##,##0") */
            if (i > 0 && (n - i) % 3 == 0) buf[o++] = ',';
            buf[o++] = digits[i];
        }
        buf[o] = '\0';
        platform_draw_text(FONT_BIG, buf, hx + hw - platform_text_width(FONT_BIG, buf), hy + (hh - th) / 2, 0x000000);
    }

    /* messages (lbl / lbl2 sit over the field in the original) */
    if (labels && (g.gameOver || g.paused || g.mode == modeDEMO)) {
        const char *l1 = g.gameOver ? " Game Over " : g.paused ? " Paused. " : " Rodent's Revenge ";
        const char *l2 = g.gameOver ? NULL : g.paused ? " Press F3 To Continue. " : " Press F2 to start ";
        int h1 = platform_text_height(FONT_BIG), h2 = l2 ? platform_text_height(FONT_NORMAL) : 0;
        int w = platform_text_width(FONT_BIG, l1);
        if (l2 && platform_text_width(FONT_NORMAL, l2) > w) w = platform_text_width(FONT_NORMAL, l2);
        int bx = fx + (fw - w) / 2 - 6, by = fy + (fh - h1 - h2) / 2 - 6;
        platform_fill_rect(bx, by, w + 12, h1 + h2 + 12, 255, 255, 255);
        platform_draw_text(FONT_BIG, l1, fx + (fw - platform_text_width(FONT_BIG, l1)) / 2, by + 6, 0x000000);
        if (l2)
            platform_draw_text(FONT_NORMAL, l2, fx + (fw - platform_text_width(FONT_NORMAL, l2)) / 2, by + 6 + h1, 0x000000);
    }
}
