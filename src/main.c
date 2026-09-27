/*
 * main.c - Rodent's Revenge entry point
 *
 * Replaces the VB1 form: the timer control `t` fires t_timer every
 * g.time milliseconds while enabled; keys go to fld_KeyDown, or to the
 * menus (menu.c) while one is open.
 *
 * Usage: rodent [--large] [--mono] [--fullscreen | --windowed] [--game DIR]
 *               [--skin DIR] [--pack FILE] [--speed 0-4] [--level N]
 *               [--dump-assets DIR] [--autostart] [--screenshot FILE]
 *               [--ticks N] [--seed N] [--show-menu NAME]
 */

#include "platform.h"
#include "assets.h"
#include "field.h"
#include "game.h"
#include "menu.h"
#include "pack.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void render_frame(void)
{
    platform_begin_frame();
    field_paint();
    hud_paint(menu_current() == MENU_NONE);
    menu_paint();
    platform_present();
}

static void usage(const char *argv0)
{
    printf("Rodent's Revenge\n");
    printf("Usage: %s [--large] [--mono] [--fullscreen | --windowed] [--game DIR]\n", argv0);
    printf("          [--skin DIR] [--data DIR] [--pack FILE] [--speed 0-4] [--level 1-50]\n");
    printf("          [--dump-assets DIR]\n\n");
    printf("  --game DIR      Folder with rodent.exe from Microsoft Entertainment Pack 2\n");
    printf("                  (default: next to this program, ., ./rodents_revenge,\n");
    printf("                  or ~/.local/share/rodentrecomp)\n");
    printf("  --pack FILE     Level pack: a file in DATA/packs or a path\n");
    printf("  --skin DIR      Replacement graphics: a folder in DATA/skins or a path,\n");
    printf("                  holding any of the PNGs --dump-assets writes\n");
    printf("  --dump-assets DIR  Save the game's graphics as PNGs for editing, then exit\n");
    printf("  --large         Use 16x16 tiles\n");
    printf("  --mono          Black and white, as on a monochrome display\n");
    printf("  --speed N       0 Snail, 1 Slow (default), 2 Medium, 3 Fast, 4 Blazing\n");
    printf("  --level N       Starting level\n");
    printf("These override the saved settings (%s) for this run.\n\n", settings_path());
    printf("Testing (saved settings are not used):\n");
    printf("  --autostart     Start a game at once\n");
    printf("  --ticks N       Run N timer ticks without waiting\n");
    printf("  --screenshot FILE  Save a PNG of the window and exit\n");
    printf("  --seed N        Fixed random seed\n");
    printf("  --show-menu NAME   title, settings, pause, gameover, scores, name or intro\n");
    printf("\nKeys: arrows/numpad/Home/End/PgUp/PgDn move, Esc menu, F2 new game, F3 pause\n");
    printf("Gamepad: d-pad or stick move (diagonals too), A select, B back, Start menu\n");
}

int main(int argc, char *argv[])
{
    Settings set, cli;
    bool autostart = false, set_large = false, set_mono = false, set_full = false;
    bool set_speed_ = false, set_level = false;
    const char *screenshot_path = NULL, *show_menu = NULL;
    const char *game_dir = NULL, *dump_dir = NULL;
    const char *data_dir = "data";
    char err[2048];
    Pack pack;
    long ticks = -1;
    unsigned seed = 0;

    settings_defaults(&cli);
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if      (!strcmp(a, "--large"))      { cli.large = true; set_large = true; }
        else if (!strcmp(a, "--mono"))       { cli.mono = true; set_mono = true; }
        else if (!strcmp(a, "--fullscreen")) { cli.fullscreen = true; set_full = true; }
        else if (!strcmp(a, "--windowed"))   { cli.fullscreen = false; set_full = true; }
        else if (!strcmp(a, "--autostart"))  autostart = true;
        else if (!strcmp(a, "--screenshot") && v) { screenshot_path = v; autostart = true; i++; }
        else if (!strcmp(a, "--show-menu") && v)  { show_menu = v; i++; }
        else if (!strcmp(a, "--game") && v)  { game_dir = v; i++; }
        else if (!strcmp(a, "--skin") && v)  { snprintf(cli.skin, sizeof cli.skin, "%s", v); i++; }
        else if (!strcmp(a, "--dump-assets") && v) { dump_dir = v; i++; }
        else if (!strcmp(a, "--data") && v)  { data_dir = v; i++; }
        else if (!strcmp(a, "--pack") && v)  { snprintf(cli.pack, sizeof cli.pack, "%s", v); i++; }
        else if (!strcmp(a, "--speed") && v) { cli.speed = atoi(v); set_speed_ = true; i++; }
        else if (!strcmp(a, "--level") && v) { cli.level = atoi(v) - 1; set_level = true; i++; }
        else if (!strcmp(a, "--ticks") && v) { ticks = atol(v); autostart = true; i++; }
        else if (!strcmp(a, "--seed") && v)  { seed = (unsigned)strtoul(v, NULL, 0); i++; }
        else if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(argv[0]); return 0; }
        else { fprintf(stderr, "Unknown option %s (try --help)\n", a); return 1; }
    }
    bool headless = ticks >= 0 || screenshot_path;
    if (show_menu && strcmp(show_menu, "pause"))  /* only the pause menu needs a game */
        autostart = false;

    /* saved settings, then the command line on top (tests use defaults only) */
    settings_defaults(&set);
    if (!headless) settings_load(&set);
    if (set_large)  set.large = cli.large;
    if (set_mono)   set.mono = cli.mono;
    if (set_full)   set.fullscreen = cli.fullscreen;
    if (set_speed_) set.speed = cli.speed < 0 ? 0 : cli.speed > 4 ? 4 : cli.speed;
    if (set_level)  set.level = cli.level < 0 ? 0 : cli.level > 49 ? 49 : cli.level;
    if (strcmp(cli.pack, "original.pack")) snprintf(set.pack, sizeof set.pack, "%s", cli.pack);
    if (cli.skin[0]) snprintf(set.skin, sizeof set.skin, "%s", cli.skin);

    if (!assets_open(game_dir, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        /* in a window too: a handheld has no terminal to show stderr */
        if (!dump_dir && !headless && platform_init(data_dir, set.large, set.mono, cFieldC, cFieldC)) {
            platform_set_fullscreen(set.fullscreen);
            platform_show_message(err);
            platform_shutdown();
        }
        return 1;
    }
    if (dump_dir) {
        bool ok = assets_dump(dump_dir);
        assets_close();
        return ok ? 0 : 1;
    }

    if (!menu_init(&set, &pack, data_dir)) {
        assets_close();
        return 1;
    }
    if (!platform_init(data_dir, set.large, set.mono, cFieldC, cFieldC) || !platform_load_graphics()) {
        fprintf(stderr, "Failed to initialize platform.\n");
        platform_shutdown();
        pack_free(&pack);
        menu_free();
        assets_close();
        return 1;
    }
    if (set.fullscreen) platform_set_fullscreen(true);

    game_init(set.speed, set.level, &pack, seed);
    set_speed(set.speed);                     /* game_init keeps the INI's 0-3 range */

    if (autostart)
        new_game();
    else if (!headless || show_menu)
        menu_show(MENU_TITLE);
    if (show_menu) {
        if (!strcmp(show_menu, "settings")) menu_show(MENU_SETTINGS);
        if (!strcmp(show_menu, "pause"))    menu_show(MENU_PAUSE);
        if (!strcmp(show_menu, "gameover")) menu_show(MENU_GAMEOVER);
        if (!strcmp(show_menu, "scores"))   menu_show(MENU_SCORES);
        if (!strcmp(show_menu, "name"))     { g.score = 12345; menu_game_over(7); }
        if (!strcmp(show_menu, "intro"))    menu_show(MENU_INTRO);
    }

    render_frame();

    if (headless) {
        /* test run: fire the timer N times as fast as possible */
        for (long n = 0; n < ticks; n++) {
            timer_tick();
            if (n % 50 == 0) render_frame();
        }
        render_frame();
        if (ticks >= 0)
            printf("after %ld ticks: mode %d level %d score %ld lives %d bads %d time %d\n",
                   ticks, g.mode, g.lvl + 1, g.score, g.cguy, g.ibadLast, g.time);
        if (screenshot_path) {
            platform_save_screenshot(screenshot_path);
            printf("Screenshot saved: %s\n", screenshot_path);
        }
    } else {
        bool running = true;
        uint32_t last_tick = platform_ticks();
        int seen_mode = g.mode;

        while (running) {
            PlatformEvent ev;
            while (running && platform_poll_event(&ev)) {
                if (ev.type == EVENT_QUIT) {
                    running = false;
                } else if (ev.type == EVENT_TEXT) {
                    menu_text(ev.key);
                } else if (ev.type == EVENT_KEYDOWN) {
                    if (menu_current() != MENU_NONE) running = menu_key(ev.key);
                    else if (ev.key == KEY_ESCAPE)   menu_show(MENU_PAUSE);
                    else if (ev.key == KEY_F2)       new_game();
                    else if (ev.key == KEY_F3)       pause_toggle();
                    else                             key_down(ev.key, ev.shift);
                    render_frame();
                }
            }
            if (!running) break;

            /* the game waits while a menu is open over it */
            uint32_t now = platform_ticks();
            int interval = g.time > 0 ? g.time : 500;
            if (menu_current() != MENU_NONE && g.mode != modeDEMO) {
                last_tick = now;
            } else if (now - last_tick >= (uint32_t)interval) {
                int before = g.mode, lvl = g.lvl;
                last_tick = now;
                timer_tick();
                if (before != modeDEMO && g.mode == modeDEMO)   /* ENDGAME -> DEMO */
                    menu_game_over(lvl + 1);
            }
            /* a level is about to begin (a new game, the next level, or the
             * level cheat): its intro card holds it until a key is pressed */
            if (g.mode == modeBEGINLEVEL && seen_mode != modeBEGINLEVEL && menu_current() == MENU_NONE)
                menu_level_start();
            seen_mode = g.mode;
            render_frame();
            platform_delay(5);
        }
    }

    field_free();
    platform_shutdown();
    pack_free(&pack);
    menu_free();
    assets_close();
    return 0;
}
