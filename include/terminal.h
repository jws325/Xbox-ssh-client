#ifndef XSSH_TERMINAL_H
#define XSSH_TERMINAL_H
#include <stdint.h>

/* 80x24 remains the default; smaller fonts can show more columns/rows. */
#define TERM_COLS 80
#define TERM_ROWS 24
#define TERM_MAX_COLS 160
#define TERM_MAX_ROWS 40

typedef struct {
    unsigned char cells[TERM_MAX_ROWS][TERM_MAX_COLS];
    uint8_t fg[TERM_MAX_ROWS][TERM_MAX_COLS];
    int cols, rows;
    int x, y, saved_x, saved_y;
    int state, params[8], argc, private_csi;
    int bright, fg_color, cursor_visible;
} Terminal;
void term_init(Terminal *t);
/* Resizes the active grid and clears the screen; call before SSH starts. */
int term_set_size(Terminal *t, int cols, int rows);
void term_feed(Terminal *t, const unsigned char *bytes, unsigned int n);
#endif
