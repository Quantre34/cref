#ifndef TERM_H
#define TERM_H

#include <sys/types.h>

typedef struct {
    unsigned char ch;       /* printable ASCII (0 = blank cell) */
    signed char   fg;       /* VT color index 0-7, -1 = default */
    signed char   bg;
    unsigned char bold    : 1;
    unsigned char reverse : 1;
} TermCell;

typedef enum {
    VT_GROUND,
    VT_ESC,
    VT_ESC_CHARSET, /* consuming charset designation byte */
    VT_CSI,
    VT_OSC,
} VtState;

typedef struct {
    int      fd;
    pid_t    pid;
    int      alive;

    int      rows, cols;
    int      crow, ccol;     /* cursor row/col */
    int      pending_wrap;   /* deferred line-wrap flag */

    TermCell *cells;         /* rows×cols grid */

    /* SGR state */
    int sgr_fg, sgr_bg;
    int sgr_bold, sgr_reverse;

    /* Scroll region */
    int scroll_top, scroll_bottom;

    /* VT parser */
    VtState state;
    int     params[16];
    int     nparams;
    char    osc_buf[256];
    int     osc_len;
} Term;

Term *term_new(void);
void  term_free(Term *t);
int   term_open(Term *t, int rows, int cols);
void  term_close(Term *t);
void  term_resize(Term *t, int rows, int cols);
void  term_pump(Term *t);
void  term_send_key(Term *t, int key);
void  term_send_raw(Term *t, const char *bytes, int n);
void  term_draw(const Term *t, int scr_y, int scr_x, int h, int w);
int   term_color_pair(int fg, int bg);
void  term_init_colors(void);

#endif
