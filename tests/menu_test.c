/*
 * menu_test.c - drives the menus with key presses and a virtual gamepad
 *
 * Run from the project root: SDL_VIDEODRIVER=dummy build/menu_test
 * (writes settings to a temporary XDG_CONFIG_HOME)
 */

#include "platform.h"
#include "assets.h"
#include "field.h"
#include "game.h"
#include "menu.h"
#include "pack.h"
#include "settings.h"
#include "scores.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

static void keys(const int *vk, int n) { for (int i = 0; i < n; i++) menu_key(vk[i]); }
#define KEYS(...) keys((int[]){ __VA_ARGS__ }, sizeof((int[]){ __VA_ARGS__ }) / sizeof(int))

static bool saved_has(const char *line)
{
    char buf[4096] = "";
    FILE *f = fopen(settings_path(), "r");
    if (!f) return false;
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    fclose(f);
    return strstr(buf, line) != NULL;
}

/* events the platform produces within ms milliseconds */
static int collect(int ms, int *out, int max)
{
    int n = 0;
    uint32_t end = SDL_GetTicks() + ms;
    while ((int32_t)(SDL_GetTicks() - end) < 0) {
        PlatformEvent ev;
        while (platform_poll_event(&ev))
            if (ev.type == EVENT_KEYDOWN && n < max) out[n++] = ev.key;
        SDL_Delay(2);
    }
    return n;
}

int main(void)
{
    char err[1024], tmp[] = "/tmp/rodent-menu-test-XXXXXX";
    Settings s;
    Pack pack;
    int ev[64], n;

    if (!mkdtemp(tmp)) return 1;
    setenv("XDG_CONFIG_HOME", tmp, 1);
    setenv("XDG_DATA_HOME", tmp, 1);
    if (!assets_open(NULL, err, sizeof err)) { printf("%s\n", err); return 1; }
    settings_defaults(&s);
    CHECK(menu_init(&s, &pack, "data"), "menu_init loads the default pack");
    platform_init("data", s.large, s.mono, cFieldC, cFieldC);
    platform_load_graphics();
    game_init(s.speed, s.level, &pack, 1);
    menu_show(MENU_TITLE);

    /* title: Play, Levels, Start at level, Settings, Quit */
    CHECK(!strcmp(s.pack, "original.pack") && !strcmp(pack.name, "Rodent's Revenge"), "starts with the original pack");
    KEYS(KEY_DOWN, KEY_RIGHT);
    CHECK(!strcmp(s.pack, "gophers-grievance.pack") && !strcmp(pack.name, "Gopher's Grievance"), "Right on Levels switches pack");
    CHECK(g.mode == modeDEMO && g.lv == &pack.levels[0], "game restarted on the new pack");
    CHECK(saved_has("pack=gophers-grievance.pack"), "pack saved");
    KEYS(KEY_LEFT);
    CHECK(!strcmp(s.pack, "original.pack"), "Left switches back");
    KEYS(KEY_DOWN, KEY_RIGHT, KEY_RIGHT);
    CHECK(s.level == 2 && g.lvlStart == 2 && g.lvl == 2, "Start at level 3, demo room redrawn");
    KEYS(KEY_LEFT, KEY_LEFT, KEY_LEFT);
    CHECK(s.level == 49, "level wraps 1 -> 50");
    KEYS(KEY_RIGHT);
    CHECK(s.level == 0 && saved_has("level=1"), "and back to 1, saved");

    /* settings: Speed, Board size, Display, Skin, Fullscreen, Back
     * (title: Play, Levels, Start at level, High scores, Settings, Quit) */
    KEYS(KEY_DOWN, KEY_DOWN, KEY_RETURN);
    CHECK(menu_current() == MENU_SETTINGS, "Enter on Settings opens it");
    KEYS(KEY_RIGHT, KEY_RIGHT, KEY_RIGHT);
    CHECK(s.speed == 4 && g.timeBase == 180 && saved_has("speed=4"), "speed to Blazing (not clamped to 0-3)");
    KEYS(KEY_DOWN, KEY_RETURN);
    CHECK(s.large && platform_tile_width() == 16 && saved_has("large=yes"), "Enter on Board size switches to Large");
    KEYS(KEY_DOWN, KEY_RIGHT);
    CHECK(s.mono && platform_mono() && g.fMono && saved_has("mono=yes"), "Display to Mono");
    KEYS(KEY_DOWN, KEY_RIGHT);
    CHECK(s.skin[0] == '\0', "Skin stays None with no skins installed");
    KEYS(KEY_ESCAPE);
    CHECK(menu_current() == MENU_TITLE, "Esc goes back to the title");

    /* play, pause, quit to title */
    KEYS(KEY_UP, KEY_UP, KEY_UP, KEY_UP);
    KEYS(KEY_RETURN);
    CHECK(menu_current() == MENU_NONE && g.mode == modeBEGINGAME, "Play starts a game");
    for (int i = 0; i < 5; i++) timer_tick();
    menu_show(MENU_PAUSE);
    KEYS(KEY_BACK);
    CHECK(menu_current() == MENU_NONE, "B / Backspace resumes from pause");
    menu_show(MENU_PAUSE);
    KEYS(KEY_DOWN, KEY_DOWN, KEY_RETURN);
    CHECK(menu_current() == MENU_TITLE && g.mode == modeDEMO, "Quit to title");
    menu_show(MENU_GAMEOVER);
    KEYS(KEY_F2);
    CHECK(menu_current() == MENU_NONE && g.mode == modeBEGINGAME, "F2 on game over plays again");

    /* saved file reloads to the same settings */
    Settings r;
    settings_defaults(&r);
    settings_load(&r);
    CHECK(r.speed == s.speed && r.large == s.large && r.mono == s.mono && r.level == s.level &&
          !strcmp(r.pack, s.pack) && !strcmp(r.skin, s.skin), "settings.ini reloads identically");

    /* level intro */
    Pack ma;
    CHECK(pack_load(&ma, "data/packs/mouse-academy.pack") && !strcmp(ma.levels[3].name, "Sinkholes") &&
          !strncmp(ma.levels[3].hint, "Cats can't cross holes", 22), "hint: parsed");
    pack_free(&ma);
    CHECK(s.intro, "level intro is on by default");
    menu_show(MENU_NONE);
    menu_level_start();
    CHECK(menu_current() == MENU_INTRO, "a level start shows the intro card");
    KEYS(KEY_NUM1 + 5);
    CHECK(menu_current() == MENU_NONE, "any key or button starts the level");
    menu_show(MENU_SETTINGS);
    for (int i = 0; i < 5; i++) KEYS(KEY_DOWN);
    KEYS(KEY_RIGHT);
    CHECK(!s.intro && saved_has("intro=no"), "Level intro can be turned off");
    menu_show(MENU_NONE);
    menu_level_start();
    CHECK(menu_current() == MENU_NONE, "and then no card appears");
    s.intro = true;

    /* high scores */
    ScoreTable t;
    menu_show(MENU_TITLE);
    g.score = 5000;
    menu_game_over(3);
    CHECK(menu_current() == MENU_NAME, "first score of a pack asks for a name");
    menu_text('B'); menu_text('o'); menu_text('b'); menu_text(' ');
    KEYS(KEY_RETURN);
    scores_load(s.pack, &t);
    CHECK(menu_current() == MENU_SCORES && t.n == 1 && !strcmp(t.s[0].name, "Bob") &&
          t.s[0].score == 5000 && t.s[0].level == 3 && t.s[0].speed == s.speed, "typed name saved (trailing space trimmed)");
    CHECK(!strcmp(s.name, "Bob") && saved_has("name=Bob"), "name remembered as the default");
    KEYS(KEY_RETURN);
    CHECK(menu_current() == MENU_GAMEOVER, "Back from the table goes to game over");

    g.score = 7000;
    menu_game_over(4);
    KEYS(KEY_BACK, KEY_BACK, KEY_BACK, KEY_NUM1 + 7, KEY_NUM1 + 7, KEY_NUM1 + 5, KEY_NUM1 + 1, KEY_NUM1 + 1);
    KEYS(KEY_RETURN);
    scores_load(s.pack, &t);
    CHECK(t.n == 2 && !strcmp(t.s[0].name, "BY") && t.s[0].score == 7000, "gamepad entry: B deletes, up/down letters, right adds one");

    for (int i = 0; i < 10; i++) {
        Score e = { .score = 100 + i, .level = 1 };
        snprintf(e.name, sizeof e.name, "Filler%d", i);
        scores_add(s.pack, &e);
    }
    scores_load(s.pack, &t);
    CHECK(t.n == SCORE_MAX && t.s[0].score == 7000 && t.s[9].score == 102, "table keeps the best 10");
    g.score = 50;
    menu_game_over(1);
    CHECK(menu_current() == MENU_GAMEOVER, "a score below the table skips the name");
    g.score = 0;
    menu_game_over(1);
    CHECK(menu_current() == MENU_GAMEOVER, "a score of 0 never counts");
    ScoreTable other;
    scores_load("gophers-grievance.pack", &other);
    CHECK(other.n == 0, "tables are per pack");

    menu_show(MENU_SCORES);
    KEYS(KEY_DOWN, KEY_RETURN);
    scores_load(s.pack, &t);
    CHECK(t.n == SCORE_MAX, "Clear asks first");
    KEYS(KEY_RETURN);
    scores_load(s.pack, &t);
    CHECK(t.n == 0, "second press clears the pack's table");

    /* gamepad: a virtual controller */
    int idx = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX,
                                        SDL_CONTROLLER_BUTTON_MAX, 0);
    SDL_Joystick *js = idx >= 0 ? SDL_JoystickOpen(idx) : NULL;
    collect(50, ev, 64);                                  /* device-added event opens it */
    if (!js || !SDL_IsGameController(idx)) {
        printf("skip: no virtual gamepad (%s)\n", SDL_GetError());
    } else {
        SDL_JoystickSetVirtualButton(js, SDL_CONTROLLER_BUTTON_DPAD_UP, 1);
        SDL_Delay(10);
        SDL_JoystickSetVirtualButton(js, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 1);
        n = collect(120, ev, 64);
        CHECK(n == 1 && ev[0] == KEY_NUM1 + 8, "Up then Right within 40 ms: one NE (numpad 9)");
        n = collect(400, ev, 64);
        CHECK(n >= 2 && ev[0] == KEY_NUM1 + 8, "held diagonal repeats");
        SDL_JoystickSetVirtualButton(js, SDL_CONTROLLER_BUTTON_DPAD_UP, 0);
        n = collect(30, ev, 64);
        CHECK(n == 1 && ev[0] == KEY_NUM1 + 5, "releasing Up while held: East at once");
        SDL_JoystickSetVirtualButton(js, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 0);
        collect(50, ev, 64);
        SDL_JoystickSetVirtualAxis(js, SDL_CONTROLLER_AXIS_LEFTX, -30000);
        SDL_JoystickSetVirtualAxis(js, SDL_CONTROLLER_AXIS_LEFTY, 29000);
        n = collect(80, ev, 64);
        CHECK(n == 1 && ev[0] == KEY_NUM1 + 0, "stick down-left: SW (numpad 1)");
        SDL_JoystickSetVirtualAxis(js, SDL_CONTROLLER_AXIS_LEFTY, 5000);
        n = collect(30, ev, 64);
        CHECK(n == 1 && ev[0] == KEY_NUM1 + 3, "stick mostly left: W (numpad 4)");
        SDL_JoystickSetVirtualAxis(js, SDL_CONTROLLER_AXIS_LEFTX, 0);
        SDL_JoystickSetVirtualAxis(js, SDL_CONTROLLER_AXIS_LEFTY, 0);
        collect(50, ev, 64);
        SDL_JoystickSetVirtualButton(js, SDL_CONTROLLER_BUTTON_A, 1);
        SDL_JoystickSetVirtualButton(js, SDL_CONTROLLER_BUTTON_START, 1);
        n = collect(30, ev, 64);
        CHECK(n == 2 && ev[0] == KEY_RETURN && ev[1] == KEY_ESCAPE, "A = Enter, Start = Esc");
        SDL_JoystickClose(js);
    }

    platform_shutdown();
    pack_free(&pack);
    menu_free();
    assets_close();
    remove(settings_path());
    remove(scores_path());
    char dir[1100];
    snprintf(dir, sizeof dir, "%s/rodentrecomp", tmp);
    rmdir(dir);
    rmdir(tmp);
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
