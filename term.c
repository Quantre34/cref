#include "term.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <signal.h>
#ifdef __APPLE__
#  include <util.h>
#elif __linux__
#  include <pty.h>
#endif

/* ── Color pairs (indices 40-103: 8 fg × 8 bg) ── */

void term_init_colors(void) {
    for (int fg = 0; fg < 8; fg++)
        for (int bg = 0; bg < 8; bg++)
            init_pair((short)(40 + fg * 8 + bg), (short)fg, (short)bg);
}

int term_color_pair(int fg, int bg) {
    int f = (fg >= 0 && fg < 8) ? fg : 7;   /* default → white (7) */
    int b = (bg >= 0 && bg < 8) ? bg : 0;   /* default → black (0) */
    return 40 + f * 8 + b;
}

/* ── Term lifecycle ── */

Term *term_new(void) {
    Term *t = calloc(1, sizeof(Term));
    if (!t) return NULL;
    t->fd     = -1;
    t->sgr_fg = -1;
    t->sgr_bg = -1;
    t->state  = VT_GROUND;
    return t;
}

void term_free(Term *t) {
    if (!t) return;
    if (t->alive) term_close(t);
    else if (t->fd >= 0) close(t->fd);
    free(t->cells);
    free(t);
}

static void fill_cells(TermCell *cells, int n) {
    for (int i = 0; i < n; i++) {
        cells[i].ch      = ' ';
        cells[i].fg      = -1;
        cells[i].bg      = -1;
        cells[i].bold    = 0;
        cells[i].reverse = 0;
    }
}

int term_open(Term *t, int rows, int cols) {
    t->rows          = rows;
    t->cols          = cols;
    t->scroll_top    = 0;
    t->scroll_bottom = rows - 1;
    t->crow = 0;
    t->ccol = 0;
    t->pending_wrap  = 0;

    t->cells = malloc(rows * cols * sizeof(TermCell));
    if (!t->cells) return -1;
    fill_cells(t->cells, rows * cols);

    struct winsize ws = { (unsigned short)rows, (unsigned short)cols, 0, 0 };
    pid_t pid = forkpty(&t->fd, NULL, NULL, &ws);
    if (pid < 0) { free(t->cells); t->cells = NULL; return -1; }

    if (pid == 0) {
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        unsetenv("TMUX");
        const char *sh = getenv("SHELL");
        if (!sh || !*sh) sh = "/bin/sh";
        execl(sh, sh, (char *)NULL);
        _exit(1);
    }

    t->pid   = pid;
    t->alive = 1;

    int fl = fcntl(t->fd, F_GETFL, 0);
    fcntl(t->fd, F_SETFL, fl | O_NONBLOCK);
    return 0;
}

void term_close(Term *t) {
    if (!t->alive) return;
    t->alive = 0;
    kill(t->pid, SIGHUP);
    if (t->fd >= 0) { close(t->fd); t->fd = -1; }
    waitpid(t->pid, NULL, WNOHANG);
}

void term_resize(Term *t, int rows, int cols) {
    if (rows == t->rows && cols == t->cols) return;

    TermCell *nc = malloc(rows * cols * sizeof(TermCell));
    if (!nc) return;
    fill_cells(nc, rows * cols);

    int min_r = rows < t->rows ? rows : t->rows;
    int min_c = cols < t->cols ? cols : t->cols;
    for (int r = 0; r < min_r; r++)
        memcpy(&nc[r * cols], &t->cells[r * t->cols], min_c * sizeof(TermCell));

    free(t->cells);
    t->cells = nc;

    t->rows          = rows;
    t->cols          = cols;
    t->scroll_top    = 0;
    t->scroll_bottom = rows - 1;
    if (t->crow >= rows) t->crow = rows - 1;
    if (t->ccol >= cols) t->ccol = cols - 1;

    if (t->alive) {
        struct winsize ws = { (unsigned short)rows, (unsigned short)cols, 0, 0 };
        ioctl(t->fd, TIOCSWINSZ, &ws);
    }
}

/* ── VT helpers ── */

static void erase_range(Term *t, int row, int c1, int c2) {
    if (row < 0 || row >= t->rows) return;
    if (c1 < 0) c1 = 0;
    if (c2 >= t->cols) c2 = t->cols - 1;
    for (int c = c1; c <= c2; c++) {
        TermCell *cell = &t->cells[row * t->cols + c];
        cell->ch = ' '; cell->fg = -1; cell->bg = -1;
        cell->bold = 0; cell->reverse = 0;
    }
}

static void scroll_up(Term *t, int n) {
    int top = t->scroll_top, bot = t->scroll_bottom;
    int h   = bot - top + 1;
    if (n <= 0) return;
    if (n >= h) {
        for (int r = top; r <= bot; r++) erase_range(t, r, 0, t->cols - 1);
        return;
    }
    memmove(&t->cells[top * t->cols],
            &t->cells[(top + n) * t->cols],
            (h - n) * t->cols * sizeof(TermCell));
    for (int r = bot - n + 1; r <= bot; r++) erase_range(t, r, 0, t->cols - 1);
}

static void scroll_down(Term *t, int n) {
    int top = t->scroll_top, bot = t->scroll_bottom;
    int h   = bot - top + 1;
    if (n <= 0) return;
    if (n >= h) {
        for (int r = top; r <= bot; r++) erase_range(t, r, 0, t->cols - 1);
        return;
    }
    memmove(&t->cells[(top + n) * t->cols],
            &t->cells[top * t->cols],
            (h - n) * t->cols * sizeof(TermCell));
    for (int r = top; r < top + n; r++) erase_range(t, r, 0, t->cols - 1);
}

static void cursor_newline(Term *t) {
    t->pending_wrap = 0;
    if (t->crow < t->scroll_bottom) {
        t->crow++;
    } else if (t->crow == t->scroll_bottom) {
        scroll_up(t, 1);
    } else {
        if (++t->crow >= t->rows) t->crow = t->rows - 1;
    }
}

static void put_char(Term *t, unsigned char c) {
    if (t->pending_wrap) {
        t->ccol = 0;
        cursor_newline(t);
    }
    if (t->crow >= 0 && t->crow < t->rows &&
        t->ccol >= 0 && t->ccol < t->cols) {
        TermCell *cell = &t->cells[t->crow * t->cols + t->ccol];
        cell->ch      = c;
        cell->fg      = (signed char)t->sgr_fg;
        cell->bg      = (signed char)t->sgr_bg;
        cell->bold    = (unsigned char)t->sgr_bold;
        cell->reverse = (unsigned char)t->sgr_reverse;
    }
    if (++t->ccol >= t->cols) {
        t->ccol = t->cols - 1;
        t->pending_wrap = 1;
    }
}

static void do_sgr(Term *t) {
    if (t->nparams == 0) {
        t->sgr_fg = -1; t->sgr_bg = -1;
        t->sgr_bold = 0; t->sgr_reverse = 0;
        return;
    }
    for (int i = 0; i < t->nparams; i++) {
        int p = t->params[i];
        if      (p == 0)              { t->sgr_fg=-1; t->sgr_bg=-1; t->sgr_bold=0; t->sgr_reverse=0; }
        else if (p == 1)              t->sgr_bold = 1;
        else if (p == 2 || p == 22)  t->sgr_bold = 0;
        else if (p == 7)             t->sgr_reverse = 1;
        else if (p == 27)            t->sgr_reverse = 0;
        else if (p >= 30 && p <= 37) t->sgr_fg = p - 30;
        else if (p == 39)            t->sgr_fg = -1;
        else if (p >= 40 && p <= 47) t->sgr_bg = p - 40;
        else if (p == 49)            t->sgr_bg = -1;
        else if (p >= 90 && p <= 97) t->sgr_fg = p - 90;    /* bright fg → same index */
        else if (p >= 100 && p <= 107) t->sgr_bg = p - 100; /* bright bg */
    }
}

static void csi_dispatch(Term *t, char final) {
    /* p1: first param with 0→1 default; p1z: first param raw (0 stays 0) */
    int p1  = (t->nparams > 0 && t->params[0]) ? t->params[0] : 1;
    int p1z = (t->nparams > 0) ? t->params[0] : 0;
    int p2  = (t->nparams > 1 && t->params[1]) ? t->params[1] : 1;

    switch (final) {
    case 'A': /* CUU */
        t->crow -= p1; if (t->crow < 0) t->crow = 0; break;
    case 'B': /* CUD */
        t->crow += p1; if (t->crow >= t->rows) t->crow = t->rows-1; break;
    case 'C': /* CUF */
        t->pending_wrap = 0;
        t->ccol += p1; if (t->ccol >= t->cols) t->ccol = t->cols-1; break;
    case 'D': /* CUB */
        t->pending_wrap = 0;
        t->ccol -= p1; if (t->ccol < 0) t->ccol = 0; break;
    case 'E': /* CNL */
        t->crow += p1; t->ccol = 0;
        if (t->crow >= t->rows) t->crow = t->rows-1; break;
    case 'F': /* CPL */
        t->crow -= p1; t->ccol = 0;
        if (t->crow < 0) t->crow = 0; break;
    case 'G': /* CHA */
        t->pending_wrap = 0;
        t->ccol = p1 - 1;
        if (t->ccol < 0) t->ccol = 0;
        if (t->ccol >= t->cols) t->ccol = t->cols-1; break;
    case 'H': case 'f': /* CUP */
        t->pending_wrap = 0;
        t->crow = p1 - 1; t->ccol = p2 - 1;
        if (t->crow < 0) t->crow = 0;
        if (t->crow >= t->rows) t->crow = t->rows-1;
        if (t->ccol < 0) t->ccol = 0;
        if (t->ccol >= t->cols) t->ccol = t->cols-1; break;
    case 'J': /* ED */
        if (p1z == 0) {
            erase_range(t, t->crow, t->ccol, t->cols-1);
            for (int r = t->crow+1; r < t->rows; r++) erase_range(t, r, 0, t->cols-1);
        } else if (p1z == 1) {
            for (int r = 0; r < t->crow; r++) erase_range(t, r, 0, t->cols-1);
            erase_range(t, t->crow, 0, t->ccol);
        } else if (p1z == 2 || p1z == 3) {
            for (int r = 0; r < t->rows; r++) erase_range(t, r, 0, t->cols-1);
        }
        break;
    case 'K': /* EL */
        if (p1z == 0) erase_range(t, t->crow, t->ccol, t->cols-1);
        else if (p1z == 1) erase_range(t, t->crow, 0, t->ccol);
        else if (p1z == 2) erase_range(t, t->crow, 0, t->cols-1);
        break;
    case 'L': /* IL: insert lines */ scroll_down(t, p1); break;
    case 'M': /* DL: delete lines */ scroll_up(t, p1);   break;
    case 'P': /* DCH: delete chars */ {
        int r = t->crow, c = t->ccol, n = p1;
        if (c + n < t->cols)
            memmove(&t->cells[r*t->cols+c], &t->cells[r*t->cols+c+n],
                    (t->cols-c-n)*sizeof(TermCell));
        erase_range(t, r, t->cols-n, t->cols-1);
        break;
    }
    case 'S': /* SU: scroll up */   scroll_up(t, p1);   break;
    case 'T': /* SD: scroll down */ scroll_down(t, p1); break;
    case 'X': /* ECH: erase chars */
        erase_range(t, t->crow, t->ccol, t->ccol+p1-1); break;
    case '@': /* ICH: insert chars */ {
        int r = t->crow, c = t->ccol, n = p1;
        if (c + n < t->cols)
            memmove(&t->cells[r*t->cols+c+n], &t->cells[r*t->cols+c],
                    (t->cols-c-n)*sizeof(TermCell));
        erase_range(t, r, c, c+n-1);
        break;
    }
    case 'd': /* VPA: vertical position absolute */
        t->crow = p1 - 1;
        if (t->crow < 0) t->crow = 0;
        if (t->crow >= t->rows) t->crow = t->rows-1; break;
    case 'r': /* DECSTBM: set scroll region */
        t->scroll_top    = (t->nparams>0 && t->params[0]) ? t->params[0]-1 : 0;
        t->scroll_bottom = (t->nparams>1 && t->params[1]) ? t->params[1]-1 : t->rows-1;
        if (t->scroll_top < 0) t->scroll_top = 0;
        if (t->scroll_bottom >= t->rows) t->scroll_bottom = t->rows-1;
        t->crow = 0; t->ccol = 0; break;
    case 'm': /* SGR */
        do_sgr(t); break;
    case 'h': case 'l':  /* private modes (alt-screen, cursor blink): ignore */
    case 'n':            /* DSR: device status — skip */
    default:
        break;
    }
}

static void term_pump_byte(Term *t, unsigned char c) {
    switch (t->state) {
    case VT_GROUND:
        if      (c == 0x1B)                        { t->state = VT_ESC; }
        else if (c == '\r')                        { t->ccol = 0; t->pending_wrap = 0; }
        else if (c=='\n' || c==0x0B || c==0x0C)   { cursor_newline(t); }
        else if (c == '\b')                        { t->pending_wrap=0; if(t->ccol>0) t->ccol--; }
        else if (c == '\t') {
            t->pending_wrap = 0;
            int next = ((t->ccol / 8) + 1) * 8;
            t->ccol = (next < t->cols) ? next : t->cols - 1;
        }
        else if (c == 0x07 || c == 0x0E || c == 0x0F) { /* BEL / SO / SI: ignore */ }
        else if (c >= 0x20 && c < 0x80) { put_char(t, c); }
        else if (c >= 0x80)             { put_char(t, '?'); }  /* non-ASCII placeholder */
        break;

    case VT_ESC:
        switch (c) {
        case '[':
            t->state = VT_CSI;
            memset(t->params, 0, sizeof(t->params));
            t->nparams = 0;
            break;
        case ']':
            t->state  = VT_OSC;
            t->osc_len = 0;
            break;
        case 'c':  /* RIS: full reset */
            fill_cells(t->cells, t->rows * t->cols);
            t->crow=0; t->ccol=0; t->pending_wrap=0;
            t->sgr_fg=-1; t->sgr_bg=-1; t->sgr_bold=0; t->sgr_reverse=0;
            t->scroll_top=0; t->scroll_bottom=t->rows-1;
            t->state = VT_GROUND;
            break;
        case 'M':  /* RI: reverse index */
            if (t->crow == t->scroll_top) scroll_down(t, 1);
            else { if (--t->crow < 0) t->crow = 0; }
            t->state = VT_GROUND;
            break;
        case 'D':  /* IND: index (same as LF) */
            cursor_newline(t);
            t->state = VT_GROUND;
            break;
        case '(': case ')': case '*': case '+':
            t->state = VT_ESC_CHARSET;
            break;
        default:
            t->state = VT_GROUND;
            break;
        }
        break;

    case VT_ESC_CHARSET:
        t->state = VT_GROUND;  /* consume single charset designation byte */
        break;

    case VT_CSI:
        if (c >= '0' && c <= '9') {
            t->params[t->nparams] = t->params[t->nparams] * 10 + (c - '0');
        } else if (c == ';') {
            if (++t->nparams >= 16) t->nparams = 15;
        } else if (c == '?' || c == '>' || c == '!') {
            /* private / intermediate marker — skip */
        } else if (c >= 0x40 && c <= 0x7E) {
            t->nparams++;  /* commit last param */
            csi_dispatch(t, (char)c);
            t->state = VT_GROUND;
        } else {
            t->state = VT_GROUND;
        }
        break;

    case VT_OSC:
        if (c == 0x07) {
            t->osc_len = 0;
            t->state   = VT_GROUND;
        } else if (c == 0x1B) {
            t->osc_len = 0;
            t->state   = VT_ESC;  /* ESC \ = ST; consume '\' in ESC state */
        } else {
            if (t->osc_len < (int)sizeof(t->osc_buf) - 1)
                t->osc_buf[t->osc_len++] = (char)c;
        }
        break;
    }
}

/* ── Pump PTY output ── */

void term_pump(Term *t) {
    if (!t->alive) return;
    unsigned char buf[4096];
    ssize_t n;
    while ((n = read(t->fd, buf, sizeof(buf))) > 0)
        for (ssize_t i = 0; i < n; i++)
            term_pump_byte(t, buf[i]);
    if (n == 0 || (n < 0 && errno == EIO))
        term_close(t);
}

/* ── Key forwarding ── */

void term_send_raw(Term *t, const char *bytes, int n) {
    if (t->alive && n > 0)
        (void)write(t->fd, bytes, n);
}

void term_send_key(Term *t, int key) {
    if (!t->alive) return;
    char buf[16];
    int  n = 0;

    if      (key >= 32 && key < 127)                        { buf[0]=(char)key; n=1; }
    else if (key == '\r' || key == '\n')                     { buf[0]='\r'; n=1; }
    else if (key >= 1 && key <= 26)                          { buf[0]=(char)key; n=1; }
    else if (key == 27)                                      { buf[0]=0x1B; n=1; }
    else if (key==127 || key==KEY_BACKSPACE || key=='\b')    { buf[0]=127; n=1; }
    else if (key == KEY_UP)    { n=snprintf(buf,sizeof(buf),"\033[A"); }
    else if (key == KEY_DOWN)  { n=snprintf(buf,sizeof(buf),"\033[B"); }
    else if (key == KEY_RIGHT) { n=snprintf(buf,sizeof(buf),"\033[C"); }
    else if (key == KEY_LEFT)  { n=snprintf(buf,sizeof(buf),"\033[D"); }
    else if (key == KEY_HOME)  { n=snprintf(buf,sizeof(buf),"\033[H"); }
    else if (key == KEY_END)   { n=snprintf(buf,sizeof(buf),"\033[F"); }
    else if (key == KEY_DC)    { n=snprintf(buf,sizeof(buf),"\033[3~"); }
    else if (key == KEY_PPAGE) { n=snprintf(buf,sizeof(buf),"\033[5~"); }
    else if (key == KEY_NPAGE) { n=snprintf(buf,sizeof(buf),"\033[6~"); }
    else if (key == KEY_IC)    { n=snprintf(buf,sizeof(buf),"\033[2~"); }
    else if (key == KEY_F(1))  { n=snprintf(buf,sizeof(buf),"\033OP"); }
    else if (key == KEY_F(2))  { n=snprintf(buf,sizeof(buf),"\033OQ"); }
    else if (key == KEY_F(3))  { n=snprintf(buf,sizeof(buf),"\033OR"); }
    else if (key == KEY_F(4))  { n=snprintf(buf,sizeof(buf),"\033OS"); }
    else if (key == KEY_F(5))  { n=snprintf(buf,sizeof(buf),"\033[15~"); }
    else if (key == KEY_F(6))  { n=snprintf(buf,sizeof(buf),"\033[17~"); }
    else if (key == KEY_F(7))  { n=snprintf(buf,sizeof(buf),"\033[18~"); }
    else if (key == KEY_F(8))  { n=snprintf(buf,sizeof(buf),"\033[19~"); }
    else if (key == KEY_F(9))  { n=snprintf(buf,sizeof(buf),"\033[20~"); }
    else if (key == KEY_F(10)) { n=snprintf(buf,sizeof(buf),"\033[21~"); }
    else if (key == KEY_F(11)) { n=snprintf(buf,sizeof(buf),"\033[23~"); }
    else if (key == KEY_F(12)) { n=snprintf(buf,sizeof(buf),"\033[24~"); }

    if (n > 0)
        (void)write(t->fd, buf, n);
}

/* ── Draw ── */

void term_draw(const Term *t, int scr_y, int scr_x, int h, int w) {
    int rows    = (h < t->rows) ? h : t->rows;
    int cols    = (w < t->cols) ? w : t->cols;
    int def_cp  = term_color_pair(-1, -1);

    for (int r = 0; r < rows; r++) {
        attron(COLOR_PAIR(def_cp));
        mvhline(scr_y + r, scr_x, ' ', w);
        attroff(COLOR_PAIR(def_cp));

        for (int c = 0; c < cols; c++) {
            const TermCell *cell = &t->cells[r * t->cols + c];
            int    cp    = term_color_pair(cell->fg, cell->bg);
            attr_t attrs = COLOR_PAIR(cp);
            if (cell->bold)    attrs |= A_BOLD;
            if (cell->reverse) attrs |= A_REVERSE;

            unsigned char ch = cell->ch;
            if (ch < 0x20 || ch == 0) ch = ' ';

            move(scr_y + r, scr_x + c);
            attron(attrs);
            addch(ch);
            attroff(attrs);
        }
    }
}
