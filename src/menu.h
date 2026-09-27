/*
 * menu.h - title, settings, pause and game-over menus (not in the original,
 * which used a Windows menu bar), drawn over the field in the style of the
 * "Game Over" label.  Keyboard: arrows, Enter, Esc/Backspace.  Gamepad:
 * d-pad or stick, A, B, Start.
 */

#ifndef MENU_H
#define MENU_H

#include <stdbool.h>
#include "settings.h"
#include "pack.h"

typedef enum {
    MENU_NONE, MENU_TITLE, MENU_SETTINGS, MENU_PAUSE, MENU_GAMEOVER,
    MENU_NAME,          /* "You have achieved a high score!" */
    MENU_SCORES         /* Hall of Fame for the current pack */
} MenuId;

/* Finds the packs and skins under data_dir, loads s->pack into *pack
 * (falling back to original.pack) and selects s->skin.  The menus change
 * *s, apply it at once and save it. */
bool menu_init(Settings *s, Pack *pack, const char *data_dir);
void menu_free(void);

MenuId menu_current(void);
void menu_show(MenuId id);
bool menu_key(int vk);            /* false: the player chose Quit */
void menu_text(int ch);           /* a typed character (name entry) */
/* After a game: name entry if the score makes the table, else game over */
void menu_game_over(int level_reached);
void menu_paint(void);

#endif /* MENU_H */
