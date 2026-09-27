/*
 * goal_test.c - checks "goal: cheese" using the packs in tests/packs
 *
 * Run from the project root: SDL_VIDEODRIVER=dummy build/goal_test tests/packs
 */

#include "platform.h"
#include "field.h"
#include "game.h"
#include "pack.h"
#include "assets.h"
#include <stdio.h>
#include <string.h>
#define RIGHT 0x27
static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)
static Pack pack;
static void start(const char *path)
{
    if (!pack_load(&pack, path)) { printf("load failed\n"); fails++; return; }
    game_init(1, 0, &pack, 42);
    new_game();
    for (int i = 0; i < 20 && g.mode != modePLAY; i++) timer_tick();
}
static void run(int n) { for (int i = 0; i < n && g.lvl == 0; i++) timer_tick(); }
int main(int argc, char **argv)
{
    const char *d = argc > 1 ? argv[1] : "tests/packs";
    char p[512];
    char err[1024];
    if (!assets_open(NULL, err, sizeof err)) { printf("%s\n", err); return 1; }
    platform_init("data", false, false, cFieldC, cFieldC);
    platform_load_graphics();

    snprintf(p, sizeof p, "%s/free.pack", d); start(p);
    CHECK(g.mode == modePLAY && g.ibadLast == 1, "free: playing with one roaming cat");
    key_down(RIGHT, 0);
    CHECK(g.mode == modePLAY, "free: first cheese eaten, level continues");
    key_down(RIGHT, 0); key_down(RIGHT, 0);
    CHECK(g.mode == modeENDLEVEL, "free: last grid cheese ends the level");
    timer_tick(); timer_tick();
    CHECK(g.lvl == 1, "free: next level");
    pack_free(&pack);

    snprintf(p, sizeof p, "%s/caged.pack", d); start(p);
    run(2000);
    CHECK(g.lvl == 0 && g.cMin == 30 && g.mode == modePLAY_FAST, "caged: cat trapped, clock held at 30 while cheese is left");
    CHECK(field_get(15, 15) == chCHEESE, "caged: trapped cat became cheese");
    long score = g.score; run(200);
    CHECK(g.score == score && g.lvl == 0, "caged: no score trickle and no level end while holding");
    key_down(RIGHT, 0); key_down(RIGHT, 0); key_down(RIGHT, 0);
    CHECK(g.mode == modeENDLEVEL, "caged: eating grid cheese ends it (trapped-cat cheese not required)");
    pack_free(&pack);

    snprintf(p, sizeof p, "%s/caged_clock.pack", d); start(p);
    run(2000);
    CHECK(g.lvl == 1, "clock goal: trapping everything still ends the level at 30");
    pack_free(&pack);

    /* goal: cats */
    snprintf(p, sizeof p, "%s/cats_caged.pack", d); start(p);
    timer_tick();
    CHECK(g.mode == modeGRAB && g.cMin == 0, "cats: trapping every cat starts the grab, no fast-forward");
    CHECK(field_get(15, 15) == chCHEESE, "cats: the cat became cheese");
    for (int i = 0; i < 49; i++) timer_tick();
    CHECK(g.mode == modeGRAB && g.lvl == 0, "cats: still waiting after 4.9 s with cheese left");
    run(5);
    CHECK(g.lvl == 1, "cats: level ends after 5 s");
    pack_free(&pack);

    snprintf(p, sizeof p, "%s/cats_caged.pack", d); start(p);
    timer_tick();
    key_down(RIGHT, 0);                       /* pushes a cage block onto the cheese */
    CHECK(field_get(15, 15) == chBLOCK, "cats: pushed block destroyed the cheese");
    timer_tick();
    CHECK(g.mode == modeENDLEVEL, "cats: no cheese left ends the level at once");
    pack_free(&pack);

    snprintf(p, sizeof p, "%s/cats_none.pack", d);
    CHECK(!pack_load(&pack, p), "cats: a level with no cats at the start is rejected");

    platform_shutdown();
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
