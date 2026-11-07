#ifndef XSSH_TERMINAL_H
#define XSSH_TERMINAL_H
#include <stdint.h>
#include <stddef.h>
#define TERM_COLOR_DEFAULT 0x02000000u
#define TERM_COLOR_RGB(r,g,b) (0x01000000u | ((uint32_t)(r)<<16) | ((uint32_t)(g)<<8) | (uint32_t)(b))
#define TERM_COLS 80
#define TERM_ROWS 24
#define TERM_MAX_COLS 160
#define TERM_MAX_ROWS 56
/* Printable bitmap characters are ASCII 32..126; internal glyphs 128+ are
   vector-drawn box strokes by the UI, preserving UTF-8 and VT100 line art. */
#define TERM_HLINE  128
#define TERM_VLINE  129
#define TERM_CROSS  130
#define TERM_UL     131
#define TERM_UR     132
#define TERM_LL     133
#define TERM_LR     134
#define TERM_TLEFT  135
#define TERM_TRIGHT 136
#define TERM_TTOP   137
#define TERM_TBOT   138

typedef struct {
    uint32_t cells[TERM_MAX_ROWS][TERM_MAX_COLS];
    uint32_t fg[TERM_MAX_ROWS][TERM_MAX_COLS];
    uint32_t bg[TERM_MAX_ROWS][TERM_MAX_COLS];
    uint8_t attr[TERM_MAX_ROWS][TERM_MAX_COLS];
    uint32_t primary_cells[TERM_MAX_ROWS][TERM_MAX_COLS];
    uint32_t primary_fg[TERM_MAX_ROWS][TERM_MAX_COLS];
    uint32_t primary_bg[TERM_MAX_ROWS][TERM_MAX_COLS];
    uint8_t primary_attr[TERM_MAX_ROWS][TERM_MAX_COLS];
    int cols,rows,x,y,saved_x,saved_y;
    int primary_x,primary_y;
    int scroll_top,scroll_bottom;
    int state,params[16],argc,private_csi;
    int bright,reverse,cursor_visible,wrap_pending,autowrap,alternate,dec_graphics;
    uint32_t fg_color,bg_color;
    int utf_remaining;
    uint32_t utf_code;
    int dirty;
    unsigned char response[128];
    unsigned int response_len;
} Terminal;
void term_init(Terminal *t);
int term_set_size(Terminal *t,int cols,int rows);
void term_feed(Terminal *t,const unsigned char *bytes,unsigned int n);
unsigned int term_take_response(Terminal *t,unsigned char *output,unsigned int maxlen);
#endif
