#include "tui.h"
#include "search.h"
#include "notes.h"

#include <sodium.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

/* Arguments passed to the background scan thread */
typedef struct {
    App        *app;
    char        dir[META_PATH_LEN];
    char        subdir_prefix[META_SUBDIR_LEN]; /* prefix to prepend to returned subdirs */
    char        exts_buf[256];
    const char *exts[16];
    int         ext_count;
    int         max_depth;  /* -1 = unlimited */
    int         merge_mode; /* 0=replace files[], 1=merge into files[] */
} ScanArgs;

/* Color pairs */
#define CP_HEADER    1
#define CP_STATUS    2
#define CP_SELECTED  3
#define CP_MD_HEAD   4
#define CP_CODE      5
#define CP_SUGGEST   6
#define CP_DIVIDER   7
#define CP_TAG       8
#define CP_DIR       9

/* File-type extension color pairs */
#define CP_EXT_C    10   /* .c */
#define CP_EXT_H    11   /* .h */
#define CP_EXT_MD   12   /* .md */
#define CP_EXT_TXT  13   /* .txt */
#define CP_EXT_PY   14   /* .py */
#define CP_EXT_JS   15   /* .js .ts .jsx .tsx */
#define CP_EXT_SH   16   /* .sh .bash .zsh */
#define CP_EXT_CFG  17   /* .json .yaml .yml .toml .xml */
#define CP_EXT_WEB  18   /* .html .css .scss */
#define CP_EXT_BIN  19   /* binary files */

/* Syntax + bat Monokai color pairs (shared by tokenizer and bat renderer) */
#define CP_BAT_KW    30   /* hot-pink  → keywords/control flow */
#define CP_BAT_TYPE  31   /* cyan      → types                 */
#define CP_BAT_DEFN  32   /* lime      → definition names      */
#define CP_BAT_STR   33   /* yellow    → strings               */
#define CP_BAT_NUM   34   /* purple    → numbers               */
#define CP_BAT_PARAM 35   /* orange    → functions/builtins    */
#define CP_BAT_CMT   36   /* dark gray → comments              */

/* Layout */
#define LEFT_W    32
#define HEADER_Y  0
#define STATUS_Y  (LINES - 1)
#define PANEL_Y   1
#define PANEL_H   (LINES - 2)
#define RIGHT_X   (LEFT_W + 1)
#define RIGHT_W   (COLS - RIGHT_X)

#define META_PANEL_H  7    /* rows for file-info panel at left-panel bottom */
#define TERM_PANEL_H 16    /* terminal panel content rows (shell sees this many rows) */
#define LNUM_W        6    /* "1234| " line-number prefix in content view */

/* Custom key codes for macOS Terminal / iTerm2 escape sequences */
#define KEY_WORD_BACK     601   /* Option+Left  */
#define KEY_WORD_FWD      602   /* Option+Right */
#define KEY_SEL_WBACK     603   /* Shift+Option+Left  */
#define KEY_SEL_WFWD      604   /* Shift+Option+Right */
#define KEY_SEL_LSTART    605   /* Shift+Cmd+Left / Shift+Home */
#define KEY_SEL_LEND      606   /* Shift+Cmd+Right / Shift+End */
#define KEY_LINE_START    607   /* Cmd+Left — jump to line start */
#define KEY_LINE_END      608   /* Cmd+Right — jump to line end */
#define KEY_LINE_MOVE_UP  609   /* Alt+Up — move line up */
#define KEY_LINE_MOVE_DN  610   /* Alt+Down — move line down */
#define KEY_SEL_PGUP      611   /* Shift+PgUp — extend selection */
#define KEY_SEL_PGDN      612   /* Shift+PgDn — extend selection */

/* Grep view column layout */
#define GREP_LBL_W   24
#define GREP_CONT_X  (GREP_LBL_W + 5)

/* Forward declarations for editor helpers used in draw functions */
static void sel_normalize(const App *app, int *r0, int *c0, int *r1, int *c1);
static void sel_clear(App *app);
static int  find_in_line_from(const char *line, const char *query, int from_col);
static const char *new_file_target_subdir(const App *app);

/* Forward declarations for notes helpers */
static void ensure_vault(App *app);
static void rebuild_notes_view(App *app);
static void load_note_content(App *app, int note_idx);
static int  save_note(App *app);

/* UTF-8 helpers --------------------------------------------------------- */

/* Byte count of a UTF-8 sequence starting with lead byte c */
static int utf8_char_bytes(unsigned char c) {
    if (c < 0x80) return 1;
    if (c < 0xC0) return 1;  /* continuation byte — treat as single */
    if (c < 0xE0) return 2;
    if (c < 0xF0) return 3;
    return 4;
}

/* Terminal display column for byte offset byte_col in s.
   Each UTF-8 character counts as 1 display column (Latin/Turkish range). */
static int utf8_display_col(const char *s, int byte_col) {
    int dcol = 0, i = 0;
    while (i < byte_col && s[i]) {
        i += utf8_char_bytes((unsigned char)s[i]);
        dcol++;
    }
    return dcol;
}

/* Print s at (y,x), clipping at max_cols display columns — no wrap-around */
static void print_clipped(int y, int x, int max_cols, const char *s) {
    if (max_cols <= 0 || !s) return;
    move(y, x);
    int dcols = 0;
    for (int i = 0; s[i] && dcols < max_cols; ) {
        int b = utf8_char_bytes((unsigned char)s[i]);
        dcols++;
        for (int k = 0; k < b && s[i + k]; k++)
            addch((unsigned char)s[i + k]);
        i += b;
    }
}

/* ---------------------------------------------------------------
   File-type color helpers
--------------------------------------------------------------- */
static const char *file_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    return (dot && dot != filename) ? dot + 1 : "";
}

static int ext_color_pair(const char *ext) {
    if (!ext || !ext[0]) return 0;
    if (strcmp(ext, "c") == 0   || strcmp(ext, "cpp") == 0 ||
        strcmp(ext, "cc") == 0  || strcmp(ext, "cxx") == 0 ||
        strcmp(ext, "go") == 0  || strcmp(ext, "rs") == 0) return CP_EXT_C;
    if (strcmp(ext, "h") == 0   || strcmp(ext, "hpp") == 0 ||
        strcmp(ext, "java") == 0 || strcmp(ext, "kt") == 0 ||
        strcmp(ext, "sql") == 0) return CP_EXT_H;
    if (strcmp(ext, "md") == 0  || strcmp(ext, "markdown") == 0) return CP_EXT_MD;
    if (strcmp(ext, "txt") == 0 || strcmp(ext, "text") == 0) return CP_EXT_TXT;
    if (strcmp(ext, "py") == 0  || strcmp(ext, "php") == 0 ||
        strcmp(ext, "lua") == 0 || strcmp(ext, "pl") == 0) return CP_EXT_PY;
    if (strcmp(ext, "js") == 0  || strcmp(ext, "ts") == 0 ||
        strcmp(ext, "jsx") == 0 || strcmp(ext, "tsx") == 0) return CP_EXT_JS;
    if (strcmp(ext, "sh") == 0  || strcmp(ext, "bash") == 0 ||
        strcmp(ext, "zsh") == 0 || strcmp(ext, "fish") == 0 ||
        strcmp(ext, "rb") == 0) return CP_EXT_SH;
    if (strcmp(ext, "json") == 0 || strcmp(ext, "yaml") == 0 ||
        strcmp(ext, "yml") == 0  || strcmp(ext, "toml") == 0 ||
        strcmp(ext, "xml") == 0) return CP_EXT_CFG;
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0 ||
        strcmp(ext, "css") == 0  || strcmp(ext, "scss") == 0) return CP_EXT_WEB;
    return 0;
}

/* ---------------------------------------------------------------
   Init / cleanup
--------------------------------------------------------------- */
void tui_init(void) {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS, NULL);
    curs_set(0);
    set_escdelay(25);   /* default 1000ms makes ESC key sluggish */

    /* Bind escape sequences to custom key codes.
       Two sets per binding: xterm-style (\033[1;Nc) and Terminal.app-style (\033x).
       For Option+Arrow to work, the terminal emulator must be configured to send
       ESC sequences: Terminal.app → Preferences → Profiles → Keyboard →
       iTerm2 (recommended): Left Option = Esc+ (Right stays Normal — non-US chars work).
       Terminal.app "Use Option as Meta Key" breaks Option+digit combos on non-US keyboards. */

    /* Option+Left/Right — word jump.
       Many sequence variants for portability across terminals. */
    define_key("\033[1;3D", KEY_WORD_BACK);   /* xterm modifyCursorKeys=2 */
    define_key("\033[1;3C", KEY_WORD_FWD);
    define_key("\033[3D",   KEY_WORD_BACK);   /* xterm modifyCursorKeys=1 (Alt) */
    define_key("\033[3C",   KEY_WORD_FWD);
    define_key("\033b",     KEY_WORD_BACK);   /* Terminal.app/iTerm2 meta */
    define_key("\033f",     KEY_WORD_FWD);
    define_key("\033[1;5D", KEY_WORD_BACK);   /* Ctrl+Left (xterm) */
    define_key("\033[1;5C", KEY_WORD_FWD);
    define_key("\033[5D",   KEY_WORD_BACK);   /* Ctrl+Left (legacy) */
    define_key("\033[5C",   KEY_WORD_FWD);

    /* Shift+Option+Left/Right — word selection */
    define_key("\033[1;4D", KEY_SEL_WBACK);   /* xterm modifyCursorKeys=2 */
    define_key("\033[1;4C", KEY_SEL_WFWD);
    define_key("\033[4D",   KEY_SEL_WBACK);   /* xterm modifyCursorKeys=1 */
    define_key("\033[4C",   KEY_SEL_WFWD);
    define_key("\033B",     KEY_SEL_WBACK);   /* Terminal.app meta (uppercase) */
    define_key("\033F",     KEY_SEL_WFWD);
    define_key("\033[1;6D", KEY_SEL_WBACK);   /* Ctrl+Shift+Left (xterm) */
    define_key("\033[1;6C", KEY_SEL_WFWD);
    define_key("\033[6D",   KEY_SEL_WBACK);   /* Ctrl+Shift+Left (legacy) */
    define_key("\033[6C",   KEY_SEL_WFWD);
    /* Terminal.app double-ESC prefix forms (Option-as-Meta sometimes emits
       \033\033[... rather than the xterm modifier syntax for Shift+Opt+Arrow). */
    define_key("\033\033[D",   KEY_WORD_BACK);    /* Alt+Left (double-ESC) */
    define_key("\033\033[C",   KEY_WORD_FWD);     /* Alt+Right */
    define_key("\033\033[1;2D", KEY_SEL_WBACK);   /* Shift+Alt+Left double-ESC */
    define_key("\033\033[1;2C", KEY_SEL_WFWD);    /* Shift+Alt+Right */

    /* Fn+Left/Right on Mac sends Home/End — already handled by KEY_HOME/KEY_END.
       We also keep Ctrl+E as a line-end alias (readline convention).
       Cmd+Left/Right are intercepted by macOS terminal apps so we do NOT bind them. */
    define_key("\005", KEY_LINE_END);   /* Ctrl+E → line end (readline-style) */
    /* NOTE: \001 (Ctrl+A) is NOT bound here — it is reserved for Select All */

    /* Shift+Home/End — select to line start/end */
    define_key("\033[1;10D", KEY_SEL_LSTART);
    define_key("\033[1;10C", KEY_SEL_LEND);
    define_key("\033[1;2H",  KEY_SEL_LSTART); /* Shift+Home */
    define_key("\033[1;2F",  KEY_SEL_LEND);   /* Shift+End */

    /* Alt+Up / Alt+Down — move line up/down (Sublime-style) */
    define_key("\033[1;3A", KEY_LINE_MOVE_UP);   /* xterm Alt+Up */
    define_key("\033[1;3B", KEY_LINE_MOVE_DN);   /* xterm Alt+Down */
    define_key("\033\033[A", KEY_LINE_MOVE_UP);  /* Terminal.app meta+Up */
    define_key("\033\033[B", KEY_LINE_MOVE_DN);  /* Terminal.app meta+Down */

    /* Shift+PgUp / Shift+PgDn — extend selection */
    define_key("\033[5;2~", KEY_SEL_PGUP);
    define_key("\033[6;2~", KEY_SEL_PGDN);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(CP_HEADER,   COLOR_WHITE,   COLOR_BLUE);
        init_pair(CP_STATUS,   COLOR_BLACK,   COLOR_CYAN);
        init_pair(CP_SELECTED, COLOR_BLACK,   COLOR_YELLOW);
        init_pair(CP_MD_HEAD,  COLOR_CYAN,    -1);
        init_pair(CP_CODE,     COLOR_GREEN,   COLOR_BLACK);
        init_pair(CP_SUGGEST,  COLOR_YELLOW,  -1);
        init_pair(CP_DIVIDER,  COLOR_WHITE,   -1);
        init_pair(CP_TAG,      COLOR_MAGENTA, -1);
        init_pair(CP_DIR,      COLOR_CYAN,    -1);
        init_pair(CP_EXT_C,    COLOR_BLUE,    -1);
        init_pair(CP_EXT_H,    COLOR_CYAN,    -1);
        init_pair(CP_EXT_MD,   COLOR_GREEN,   -1);
        init_pair(CP_EXT_TXT,  -1,            -1);
        init_pair(CP_EXT_PY,   COLOR_YELLOW,  -1);
        init_pair(CP_EXT_JS,   COLOR_YELLOW,  -1);
        init_pair(CP_EXT_SH,   COLOR_MAGENTA, -1);
        init_pair(CP_EXT_CFG,  COLOR_CYAN,    -1);
        init_pair(CP_EXT_WEB,  COLOR_BLUE,    -1);
        init_pair(CP_EXT_BIN,  COLOR_RED,     -1);
        init_pair(CP_BAT_KW,    COLOR_RED,     -1);
        init_pair(CP_BAT_TYPE,  COLOR_CYAN,    -1);
        init_pair(CP_BAT_DEFN,  COLOR_GREEN,   -1);
        init_pair(CP_BAT_STR,   COLOR_YELLOW,  -1);
        init_pair(CP_BAT_NUM,   COLOR_MAGENTA, -1);
        init_pair(CP_BAT_PARAM, COLOR_YELLOW,  -1);
        init_pair(CP_BAT_CMT,   COLOR_WHITE,   -1);
        term_init_colors();  /* pairs 40-103 for PTY terminal fg×bg */
    }
}

void tui_cleanup(void) {
    endwin();
}

void content_free(Content *c) {
    for (int i = 0; i < c->line_count; i++) free(c->lines[i]);
    free(c->lines);
    c->lines      = NULL;
    c->line_count = 0;
    c->capacity   = 0;
}

static void content_append_line_raw(Content *c, const char *text) {
    if (c->line_count >= c->capacity) {
        int cap = c->capacity ? c->capacity * 2 : 64;
        char **tmp = realloc(c->lines, cap * sizeof(char *));
        if (!tmp) return;
        c->lines    = tmp;
        c->capacity = cap;
    }
    c->lines[c->line_count++] = strdup(text ? text : "");
}

void clipboard_free(App *a) {
    for (int i = 0; i < a->clipboard_count; i++) free(a->clipboard[i]);
    free(a->clipboard);
    a->clipboard       = NULL;
    a->clipboard_count = 0;
}

/* ---------------------------------------------------------------
   Case-insensitive substring search.
   Returns 1 if found, sets *offset to the byte position.
--------------------------------------------------------------- */
static int ci_find(const char *hay, const char *needle, int *offset) {
    int nlen = (int)strlen(needle);
    if (nlen == 0) return 0;
    int hlen = (int)strlen(hay);
    for (int i = 0; i <= hlen - nlen; i++) {
        int ok = 1;
        for (int j = 0; j < nlen; j++) {
            if (tolower((unsigned char)hay[i+j]) !=
                tolower((unsigned char)needle[j])) { ok = 0; break; }
        }
        if (ok) { if (offset) *offset = i; return 1; }
    }
    return 0;
}

/* ---------------------------------------------------------------
   Collect grep hits across all loaded files.
--------------------------------------------------------------- */
static void grep_collect(App *app) {
    app->grep_hit_count = 0;
    app->grep_sel       = 0;
    app->grep_offset    = 0;
    if (!app->grep_query[0]) return;

    for (int fi = 0; fi < app->file_count; fi++) {
        FILE *f = fopen(app->files[fi].path, "r");
        if (!f) continue;

        char raw[1024];
        int  lineno = 0;

        while (fgets(raw, sizeof(raw), f) &&
               app->grep_hit_count < GREP_MAX_HITS) {
            lineno++;
            raw[strcspn(raw, "\r\n")] = '\0';

            int off;
            if (ci_find(raw, app->grep_query, &off)) {
                GrepHit *h      = &app->grep_hits[app->grep_hit_count++];
                h->file_idx     = fi;
                h->line_no      = lineno;
                h->match_col    = off;
                h->match_len    = (int)strlen(app->grep_query);
                strncpy(h->line, raw, sizeof(h->line) - 1);
                h->line[sizeof(h->line) - 1] = '\0';
            }
        }
        fclose(f);
    }
}

/* ---------------------------------------------------------------
   Tree helpers
--------------------------------------------------------------- */

/* qsort comparator for fixed-width string arrays */
static int qsort_strcmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

/* Binary search: first index where files[i].subdir >= target */
static int file_lower_bound(const FileMeta *files, int n, const char *target) {
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (strcmp(files[mid].subdir, target) < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

/* Binary search: first index where dirs[i] >= target */
static int dir_lower_bound(char (*dirs)[META_SUBDIR_LEN], int n, const char *target) {
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (strcmp(dirs[mid], target) < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

/* Count files whose subdir equals subdir or is a descendant of it.
   Uses binary search for O(log N + results) per call. */
static int count_files_in_subdir(const App *app, const char *subdir) {
    int n = 0;
    int sl = (int)strlen(subdir);

    /* Range 1: files with subdir == subdir exactly */
    int fi0 = file_lower_bound(app->files, app->file_count, subdir);
    for (int fi = fi0; fi < app->file_count; fi++) {
        if (strcmp(app->files[fi].subdir, subdir) != 0) break;
        n++;
    }

    /* Range 2: files with subdir starting with subdir+"/" */
    char prefix[META_SUBDIR_LEN];
    strncpy(prefix, subdir, sizeof(prefix) - 2);
    prefix[sizeof(prefix) - 2] = '\0';
    int plen = (int)strlen(prefix);
    prefix[plen]     = '/';
    prefix[plen + 1] = '\0';

    int fi1 = file_lower_bound(app->files, app->file_count, prefix);
    for (int fi = fi1; fi < app->file_count; fi++) {
        const char *s = app->files[fi].subdir;
        if (strncmp(s, prefix, (size_t)(sl + 1)) != 0) break;
        n++;
    }
    return n;
}

/* Returns number of nodes added to tree_all (children only, not the dir node itself).
   dirs[] must be sorted. Uses binary search to find descendants. */
static int build_subtree(App *app, const char *prefix, int plen, int depth,
                          char (*dirs)[META_SUBDIR_LEN], int dir_count) {
    int added = 0;

    /* Binary-search into dirs[] for the first descendant of prefix */
    int di;
    if (plen == 0) {
        di = 0;
    } else {
        char marker[META_SUBDIR_LEN];
        int mlen = plen < (int)(sizeof(marker) - 2) ? plen : (int)(sizeof(marker) - 2);
        strncpy(marker, prefix, (size_t)mlen);
        marker[mlen]     = '/';
        marker[mlen + 1] = '\0';
        di = dir_lower_bound(dirs, dir_count, marker);
    }

    /* Iterate direct-child dirs — all descendants follow contiguously after prefix+"/" */
    while (di < dir_count && app->tree_all_count < TREE_MAX_NODES) {
        const char *d = dirs[di];

        /* Stop when d is no longer a descendant of prefix */
        if (plen > 0 && (strncmp(d, prefix, plen) != 0 || d[plen] != '/')) break;

        const char *suffix = (plen > 0) ? d + plen + 1 : d;
        if (strchr(suffix, '/') != NULL) {
            /* Grandchild — not a direct child, skip */
            di++;
            continue;
        }

        /* Direct child: add DIR node */
        int node_idx = app->tree_all_count++;
        TreeNode *n  = &app->tree_all[node_idx];
        memset(n, 0, sizeof(*n));
        n->kind     = NODE_DIR;
        n->depth    = depth;
        n->expanded = 0;
        n->file_idx = -1;

        const char *slash = strrchr(d, '/');
        strncpy(n->name,   slash ? slash + 1 : d, sizeof(n->name)   - 1);
        strncpy(n->subdir, d,                      sizeof(n->subdir) - 1);

        int dlen = (int)strlen(d);
        int sub  = build_subtree(app, d, dlen, depth + 1, dirs, dir_count);
        n->subtree_size = sub;
        n->total_files  = count_files_in_subdir(app, d);
        added += 1 + sub;

        /* Skip past d's subtree in dirs[] */
        di++;
        while (di < dir_count &&
               strncmp(dirs[di], d, dlen) == 0 && dirs[di][dlen] == '/') {
            di++;
        }
    }

    /* Files directly in this prefix — O(log N + files_here) via binary search */
    int fi0 = file_lower_bound(app->files, app->file_count, prefix);
    for (int fi = fi0; fi < app->file_count; fi++) {
        if (strcmp(app->files[fi].subdir, prefix) != 0) break;
        if (app->tree_all_count >= TREE_MAX_NODES) break;

        int node_idx = app->tree_all_count++;
        TreeNode *fn = &app->tree_all[node_idx];
        memset(fn, 0, sizeof(*fn));
        fn->kind     = NODE_FILE;
        fn->depth    = depth;
        fn->file_idx = fi;
        strncpy(fn->name, app->files[fi].filename, sizeof(fn->name) - 1);
        added++;
    }

    return added;
}

static void rebuild_view(App *app) {
    app->tree_view_count = 0;
    int i = 0;
    while (i < app->tree_all_count && app->tree_view_count < TREE_MAX_NODES) {
        app->tree_view[app->tree_view_count++] = i;
        if (app->tree_all[i].kind == NODE_DIR && !app->tree_all[i].expanded)
            i += app->tree_all[i].subtree_size + 1;
        else
            i++;
    }
}

static void build_tree(App *app) {
    /* Snapshot expanded dirs and currently-selected subdir before wiping tree */
    char (*expanded_snap)[META_SUBDIR_LEN] = NULL;
    int   expanded_snap_count = 0;
    char  sel_subdir[META_SUBDIR_LEN]  = "";
    char  sel_name[64]                 = "";

    if (app->tree_all_count > 0) {
        expanded_snap = malloc(app->tree_all_count * sizeof(*expanded_snap));
        if (expanded_snap) {
            for (int i = 0; i < app->tree_all_count; i++) {
                if (app->tree_all[i].kind == NODE_DIR &&
                    app->tree_all[i].expanded) {
                    strncpy(expanded_snap[expanded_snap_count++],
                            app->tree_all[i].subdir, META_SUBDIR_LEN - 1);
                }
            }
        }
        /* Record currently selected item for re-selection after rebuild */
        if (app->tree_sel < app->tree_view_count) {
            int vi = app->tree_view[app->tree_sel];
            strncpy(sel_subdir, app->tree_all[vi].subdir, META_SUBDIR_LEN - 1);
            strncpy(sel_name,   app->tree_all[vi].name,   sizeof(sel_name) - 1);
        }
    }

    app->tree_all_count  = 0;
    app->tree_view_count = 0;
    app->tree_sel        = 0;
    app->tree_offset     = 0;

    if (app->file_count == 0 && app->boundary_dir_count == 0) {
        free(expanded_snap);
        return;
    }

    /* Step 1: Collect unique subdirs from sorted files — O(N)
       Since files[] is sorted by subdir, duplicates are consecutive. */
    int usub_cap = app->file_count + 1;
    char (*usubs)[META_SUBDIR_LEN] = malloc(usub_cap * sizeof(*usubs));
    if (!usubs) { free(expanded_snap); return; }
    int usub_count = 0;
    const char *prev_sub = "";
    for (int fi = 0; fi < app->file_count; fi++) {
        const char *sub = app->files[fi].subdir;
        if (!sub[0]) continue;
        if (strcmp(sub, prev_sub) == 0) continue;
        prev_sub = app->files[fi].subdir;
        strncpy(usubs[usub_count++], sub, META_SUBDIR_LEN - 1);
    }

    /* Step 2: Generate ALL prefix strings — O(D × depth).
       Also include all boundary dirs (and their prefixes) so they
       appear in the tree even though they have no files yet. */
    int prefix_cap = (usub_count + app->boundary_dir_count) * 12 + 16;
    char (*prefixes)[META_SUBDIR_LEN] = malloc(prefix_cap * sizeof(*prefixes));
    if (!prefixes) { free(usubs); free(expanded_snap); return; }
    int prefix_count = 0;

    /* Helper lambda-like: add all prefixes of a path string */
    /* — inlined for both usubs and boundary_dirs */
    for (int ui = 0; ui < usub_count; ui++) {
        char buf[META_SUBDIR_LEN];
        strncpy(buf, usubs[ui], sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *p = buf;
        while (1) {
            char *sl = strchr(p, '/');
            if (sl) *sl = '\0';
            if (prefix_count < prefix_cap)
                strncpy(prefixes[prefix_count++], buf, META_SUBDIR_LEN - 1);
            if (!sl) break;
            *sl = '/'; p = sl + 1;
        }
    }
    free(usubs);

    for (int bi = 0; bi < app->boundary_dir_count; bi++) {
        char buf[META_SUBDIR_LEN];
        strncpy(buf, app->boundary_dirs[bi], sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *p = buf;
        while (1) {
            char *sl = strchr(p, '/');
            if (sl) *sl = '\0';
            if (prefix_count < prefix_cap)
                strncpy(prefixes[prefix_count++], buf, META_SUBDIR_LEN - 1);
            if (!sl) break;
            *sl = '/'; p = sl + 1;
        }
    }

    /* Step 3: Sort prefixes — O(P log P) */
    qsort(prefixes, prefix_count, sizeof(*prefixes), qsort_strcmp);

    /* Step 4: Unique → dirs[] — O(P) */
    char (*dirs)[META_SUBDIR_LEN] = malloc((prefix_count + 1) * sizeof(*dirs));
    if (!dirs) { free(prefixes); free(expanded_snap); return; }
    int dir_count = 0;
    for (int i = 0; i < prefix_count; i++) {
        if (dir_count == 0 || strcmp(prefixes[i], dirs[dir_count - 1]) != 0) {
            strncpy(dirs[dir_count++], prefixes[i], META_SUBDIR_LEN - 1);
        }
    }
    free(prefixes);

    /* dirs[] is sorted; build_subtree uses binary search to find descendants */
    build_subtree(app, "", 0, 0, dirs, dir_count);
    free(dirs);

    /* Step 5: Restore expanded state */
    if (expanded_snap) {
        for (int i = 0; i < app->tree_all_count; i++) {
            if (app->tree_all[i].kind != NODE_DIR) continue;
            for (int j = 0; j < expanded_snap_count; j++) {
                if (strcmp(app->tree_all[i].subdir, expanded_snap[j]) == 0) {
                    app->tree_all[i].expanded = 1;
                    break;
                }
            }
        }
        free(expanded_snap);
    }

    rebuild_view(app);

    /* Step 6: Restore selection (best-effort match by subdir+name) */
    if (sel_subdir[0] || sel_name[0]) {
        int best = -1;
        for (int i = 0; i < app->tree_view_count; i++) {
            int vi = app->tree_view[i];
            if (strcmp(app->tree_all[vi].subdir, sel_subdir) == 0 &&
                strcmp(app->tree_all[vi].name,   sel_name)   == 0) {
                best = i;
                break;
            }
        }
        if (best >= 0) {
            app->tree_sel = best;
            /* Keep offset sane */
            int view_h = PANEL_H - META_PANEL_H - 1;
            if (app->tree_sel < app->tree_offset)
                app->tree_offset = app->tree_sel;
            if (app->tree_sel >= app->tree_offset + view_h)
                app->tree_offset = app->tree_sel - view_h + 1;
            if (app->tree_offset < 0) app->tree_offset = 0;
        }
    }
}

/* ---------------------------------------------------------------
   Header bar
--------------------------------------------------------------- */
static void draw_header(const App *app) {
    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvhline(HEADER_Y, 0, ' ', COLS);
    mvprintw(HEADER_Y, 1, " cref");
    attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

    /* Show current scan directory or Library indicator */
    {
        char dir_label[40] = "";
        if (app->in_lib_view) {
            strncpy(dir_label, "[Library]", sizeof(dir_label) - 1);
        } else if (app->scan_dir_path[0]) {
            const char *sl = strrchr(app->scan_dir_path, '/');
            const char *base = sl ? sl + 1 : app->scan_dir_path;
            if (!base[0]) base = app->scan_dir_path; /* root "/" case */
            snprintf(dir_label, sizeof(dir_label), "[%s]", base);
        }
        if (dir_label[0]) {
            attron(COLOR_PAIR(CP_HEADER));
            mvprintw(HEADER_Y, 7, "%-18.18s", dir_label);
            attroff(COLOR_PAIR(CP_HEADER));
        }
    }

    int sx = COLS / 2 - 20;
    if (sx < 10) sx = 10;

    if (app->mode == MODE_GREP) {
        attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
        mvprintw(HEADER_Y, sx, "[--grep] %-28s", app->grep_query);
        attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);
    } else {
        attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
        mvprintw(HEADER_Y, sx, "[/] Search: ");
        attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

        attron(COLOR_PAIR(CP_HEADER));
        char shown[40];
        snprintf(shown, sizeof(shown), "%-26s", app->query);
        mvprintw(HEADER_Y, sx + 12, "%s", shown);

        if (app->mode == MODE_SEARCH) {
            attron(A_BLINK | A_BOLD);
            mvaddch(HEADER_Y, sx + 12 + app->query_len, '_');
            attroff(A_BLINK | A_BOLD);
        }
        attroff(COLOR_PAIR(CP_HEADER));
    }

    attron(COLOR_PAIR(CP_HEADER));
    mvprintw(HEADER_Y, COLS - 12, " [q]Quit [?]");
    attroff(COLOR_PAIR(CP_HEADER));
}

/* ---------------------------------------------------------------
   Status bar
--------------------------------------------------------------- */
static void draw_status(const App *app) {
    attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
    mvhline(STATUS_Y, 0, ' ', COLS);

    if (app->mode == MODE_EDIT_FIND) {
        char prompt[256];
        snprintf(prompt, sizeof(prompt),
                 " Find: %s",
                 app->find_query);
        mvprintw(STATUS_Y, 1, "%-*s", COLS - 16, prompt);
        /* show blinking cursor at end of query */
        attron(A_BLINK | A_BOLD);
        mvaddch(STATUS_Y, 8 + app->find_query_len, '_');
        attroff(A_BLINK | A_BOLD);
        /* show match count */
        attron(A_BOLD);
        char mc[24];
        if (app->find_query_len == 0)
            snprintf(mc, sizeof(mc), " [Esc] cancel ");
        else
            snprintf(mc, sizeof(mc), " %d matches ", app->find_match_count);
        mvprintw(STATUS_Y, COLS - (int)strlen(mc) - 1, "%s", mc);
        attroff(A_BOLD);
    } else if (app->mode == MODE_EDIT) {
        mvprintw(STATUS_Y, 1,
            " [Esc]Read [^S]Save [^B]Build [^Z]Undo [^X/^C/^V]Cut "
            "[^A]All [^L]Sel [^D]Dup [^K]Del [^F]Find");
        attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
        /* Diagnostic: key code + Ln/Col + SEL indicator */
        if (app->content.line_count > 0) {
            const char *cl = app->content.lines[app->cursor_row];
            int dcol = utf8_display_col(cl, app->cursor_col);
            /* High-contrast key code on the right edge */
            char kbuf[24];
            snprintf(kbuf, sizeof(kbuf), " key=%d%s ",
                     app->last_key,
                     app->sel_active ? " SEL" : "");
            attron(A_BOLD | COLOR_PAIR(CP_SUGGEST));
            mvprintw(STATUS_Y, COLS - (int)strlen(kbuf) - 14, "%s", kbuf);
            attroff(A_BOLD | COLOR_PAIR(CP_SUGGEST));
            /* Ln/Col next to it */
            attron(A_BOLD);
            char lnbuf[32];
            snprintf(lnbuf, sizeof(lnbuf), " Ln %d Col %d ",
                     app->cursor_row + 1, dcol + 1);
            mvprintw(STATUS_Y, COLS - (int)strlen(kbuf) - 14 - (int)strlen(lnbuf),
                     "%s", lnbuf);
            attroff(A_BOLD);
        }
        if (app->save_flash > 0) {
            attron(A_BOLD | COLOR_PAIR(CP_CODE));
            mvprintw(STATUS_Y, COLS - 10, " Saved! ");
            attroff(A_BOLD | COLOR_PAIR(CP_CODE));
        } else if (app->content_dirty) {
            attron(A_BOLD | COLOR_PAIR(CP_SUGGEST));
            mvprintw(STATUS_Y, COLS - 12, " [unsaved]");
            attroff(A_BOLD | COLOR_PAIR(CP_SUGGEST));
        }
        attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
    } else if (app->mode == MODE_SEARCH) {
        mvprintw(STATUS_Y, 1,
            " [Enter/Esc] Done  [↑↓] Navigate  "
            "[Backspace] Delete  [Ctrl+U] Clear");
        if (app->filtered_count == 0 && app->suggestion[0]) {
            attroff(COLOR_PAIR(CP_STATUS));
            attron(COLOR_PAIR(CP_SUGGEST) | A_BOLD);
            int ox = COLS - (int)strlen(app->suggestion) - 22;
            if (ox > 2)
                mvprintw(STATUS_Y, ox, " Did you mean: \"%s\"?", app->suggestion);
            attroff(COLOR_PAIR(CP_SUGGEST) | A_BOLD);
        }
    } else if (app->mode == MODE_CONTENT) {
        mvprintw(STATUS_Y, 1,
            " [↑↓/PgUp/PgDn] Scroll  [←/Esc] Back  "
            "[[/]] Section  [g/G] Top/Bot  [e] Edit  [q] Quit");
        if (app->open_file >= 0 && app->content.line_count > 0) {
            int pct = (app->content_offset + 1) * 100 /
                       app->content.line_count;
            attron(A_BOLD);
            mvprintw(STATUS_Y, COLS - 8, "[%3d%%]", pct);
            attroff(A_BOLD);
        }
    } else if (app->mode == MODE_GREP) {
        mvprintw(STATUS_Y, 1,
            " [↑↓/jk] Scroll  [Enter/→] Open file  "
            "[←/Esc] Back  [q] Quit");
        attron(A_BOLD);
        mvprintw(STATUS_Y, COLS - 12, "[%d/%d]",
            app->grep_hit_count > 0 ? app->grep_sel + 1 : 0,
            app->grep_hit_count);
        attroff(A_BOLD);
    } else if (app->mode == MODE_FILTER) {
        mvprintw(STATUS_Y, 1, " Filter ext: %s", app->filter_input);
        attron(A_BLINK | A_BOLD);
        mvaddch(STATUS_Y, 14 + app->filter_input_len, '_');
        attroff(A_BLINK | A_BOLD);
        attron(A_BOLD);
        mvprintw(STATUS_Y, COLS - 27, "(Enter=apply  Esc=cancel)");
        attroff(A_BOLD);
    } else if (app->mode == MODE_GOTO) {
        mvprintw(STATUS_Y, 1, " Go to: %s", app->goto_buf);
        attron(A_BLINK | A_BOLD);
        mvaddch(STATUS_Y, 9 + app->goto_len, '_');
        attroff(A_BLINK | A_BOLD);
        attron(A_BOLD);
        mvprintw(STATUS_Y, COLS - 27, "(Enter=open  Esc=cancel)");
        attroff(A_BOLD);
    } else if (app->mode == MODE_NEW_FILE) {
        const char *sub = new_file_target_subdir(app);
        if (sub[0])
            mvprintw(STATUS_Y, 1, " New file in %s/: %s", sub, app->new_file_buf);
        else
            mvprintw(STATUS_Y, 1, " New file: %s", app->new_file_buf);
        int prefix = sub[0] ? 17 + (int)strlen(sub) : 11;
        attron(A_BLINK | A_BOLD);
        mvaddch(STATUS_Y, prefix + app->new_file_len, '_');
        attroff(A_BLINK | A_BOLD);
        if (app->new_file_msg[0]) {
            attron(A_BOLD | COLOR_PAIR(CP_EXT_BIN));
            mvprintw(STATUS_Y, COLS - (int)strlen(app->new_file_msg) - 2,
                     "%s", app->new_file_msg);
            attroff(A_BOLD | COLOR_PAIR(CP_EXT_BIN));
        } else {
            attron(A_BOLD);
            mvprintw(STATUS_Y, COLS - 27, "(Enter=create  Esc=cancel)");
            attroff(A_BOLD);
        }
    } else if (app->mode == MODE_CONFIRM_DEL) {
        /* Use stored path/filename — index may be stale after a rescan */
        const char *fpath = app->del_pending_path[0] ? app->del_pending_path : "?";
        const char *slash = strrchr(fpath, '/');
        const char *fname = slash ? slash + 1 : fpath;
        attron(A_BOLD | COLOR_PAIR(CP_EXT_BIN));
        mvprintw(STATUS_Y, 1, " Delete \"%s\"? [y/N]", fname);
        attroff(A_BOLD | COLOR_PAIR(CP_EXT_BIN));
    } else if (app->mode == MODE_COMPILE_OUT) {
        if (app->compile_ok)
            attron(A_BOLD | COLOR_PAIR(CP_BAT_DEFN));
        else
            attron(A_BOLD | COLOR_PAIR(CP_EXT_BIN));
        mvprintw(STATUS_Y, 1, " %s", app->compile_status);
        attroff(A_BOLD | COLOR_PAIR(CP_BAT_DEFN) | COLOR_PAIR(CP_EXT_BIN));
        attron(A_BOLD);
        mvprintw(STATUS_Y, COLS - 38, " [↑↓] Scroll  [^B] Rebuild  [Esc] Back");
        attroff(A_BOLD);
    } else {
        /* MODE_LIST: dispatch on query */
        if (app->query[0] == '\0') {
            mvprintw(STATUS_Y, 1,
                " [↑↓] Navigate  [→] Open  [/] Search  "
                "[^N] New  [^D] Del  [^T] Filter  [^L] Lib  [^G] Goto  [q] Quit");
            attron(A_BOLD);
            if (app->scan_running) {
                static const char sp[] = "|/-\\";
                const char *lbl = app->scan_full ? "yükleniyor" : "hızlı tarama";
                mvprintw(STATUS_Y, COLS - 22, " %c %d %s  ",
                         sp[(app->scan_frame / 3) % 4], app->scan_live_count, lbl);
            } else {
                mvprintw(STATUS_Y, COLS - 12, "[%d/%d]",
                    app->tree_view_count > 0 ? app->tree_sel + 1 : 0,
                    app->tree_view_count);
            }
            attroff(A_BOLD);
        } else {
            mvprintw(STATUS_Y, 1,
                " [↑↓] Select  [→/Enter] Open  [/] Search  [^T] Filter  [q] Quit");
            attron(A_BOLD);
            if (app->scan_running) {
                static const char sp[] = "|/-\\";
                const char *lbl = app->scan_full ? "yükleniyor" : "hızlı tarama";
                mvprintw(STATUS_Y, COLS - 22, " %c %d %s  ",
                         sp[(app->scan_frame / 3) % 4], app->scan_live_count, lbl);
            } else {
                mvprintw(STATUS_Y, COLS - 12, "[%d/%d]",
                    app->filtered_count > 0 ? app->list_sel + 1 : 0,
                    app->filtered_count);
            }
            attroff(A_BOLD);
        }
    }

    attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
}

/* ---------------------------------------------------------------
   Left panel: tree view (no query active)
--------------------------------------------------------------- */
static void draw_list_tree(const App *app) {
    for (int r = PANEL_Y; r < PANEL_Y + PANEL_H; r++)
        mvhline(r, 0, ' ', LEFT_W);

    attron(A_BOLD | A_UNDERLINE);
    if (app->scan_running) {
        static const char sp[] = "|/-\\";
        char hdr[64];
        snprintf(hdr, sizeof(hdr), "Files %c", sp[(app->scan_frame / 3) % 4]);
        mvprintw(PANEL_Y, 1, "%-*s", LEFT_W - 2, hdr);
    } else {
        mvprintw(PANEL_Y, 1, "%-*s", LEFT_W - 2, "Files");
    }
    attroff(A_BOLD | A_UNDERLINE);

    if (app->tree_view_count == 0) {
        attron(COLOR_PAIR(CP_SUGGEST));
        if (app->scan_running) {
            static const char sp[] = "|/-\\";
            mvprintw(PANEL_Y + 1, 2, "%c yükleniyor...",
                     sp[(app->scan_frame / 3) % 4]);
        } else {
            mvprintw(PANEL_Y + 1, 2, "No files found.");
        }
        attroff(COLOR_PAIR(CP_SUGGEST));
        return;
    }

    int view_h = PANEL_H - META_PANEL_H - 1;
    int base_y = PANEL_Y + 1;

    for (int i = 0; i < view_h && i + app->tree_offset < app->tree_view_count; i++) {
        int vi           = app->tree_view[i + app->tree_offset];
        const TreeNode *n = &app->tree_all[vi];
        int row   = base_y + i;
        int is_sel  = (i + app->tree_offset == app->tree_sel);
        int is_open = is_sel && (app->mode == MODE_CONTENT || app->mode == MODE_EDIT);

        int pair = 0;
        if      (is_sel && !is_open)  pair = CP_SELECTED;
        else if (is_open)             pair = CP_MD_HEAD;
        else if (n->kind == NODE_DIR) pair = CP_DIR;
        if (pair) attron(COLOR_PAIR(pair) | A_BOLD);

        const char *arrow = is_sel ? "▶" : " ";
        /* available width after arrow(1)+space(1)+indent */
        int avail = LEFT_W - 2 - n->depth * 2 - 1;
        if (avail < 4) avail = 4;

        if (n->kind == NODE_DIR) {
            char info[12];
            snprintf(info, sizeof(info), "(%d)", n->total_files);
            int info_w = (int)strlen(info);
            /* icon "[+]" = 3 chars, space = 1 */
            int name_w = avail - 3 - 1 - info_w - 1;
            if (name_w < 1) name_w = 1;

            char name[64];
            strncpy(name, n->name, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            if ((int)strlen(name) > name_w) name[name_w] = '\0';

            /* Show spinner glyph when this dir is currently being loaded */
            char bracket_char = n->expanded ? '-' : '+';
            if (app->loading_subdir[0] &&
                strcmp(n->subdir, app->loading_subdir) == 0) {
                static const char sp[] = "|/-\\";
                bracket_char = sp[(app->scan_frame / 3) % 4];
            }
            mvprintw(row, 0, "%s %*s[%c]%-*s %s",
                arrow,
                n->depth * 2, "",
                bracket_char,
                name_w, name,
                info);
        } else {
            int fi = n->file_idx;
            int lc     = (fi >= 0) ? app->files[fi].line_count  : 0;
            int is_bin = (fi >= 0) ? app->files[fi].binary       : 0;
            int no_rd  = (fi >= 0) ? app->files[fi].unreadable   : 0;
            char info[12] = "";
            if (no_rd)  snprintf(info, sizeof(info), " ---");
            else if (is_bin) snprintf(info, sizeof(info), " bin");
            else if (lc > 0) snprintf(info, sizeof(info), " %dL", lc);
            int info_w = (int)strlen(info);
            int name_w = avail - info_w;
            if (name_w < 1) name_w = 1;

            char name[64];
            strncpy(name, n->name, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            if ((int)strlen(name) > name_w) name[name_w] = '\0';

            /* Dim unreadable files; apply ext color only when not selected */
            if (no_rd && !pair) attron(A_DIM);
            int ext_pair = 0;
            if (!pair && fi >= 0) {
                if (is_bin || no_rd)
                    ext_pair = CP_EXT_BIN;
                else
                    ext_pair = ext_color_pair(file_ext(app->files[fi].filename));
            }
            if (ext_pair) attron(COLOR_PAIR(ext_pair));

            mvprintw(row, 0, "%s %*s%-*s%s",
                arrow,
                n->depth * 2, "",
                name_w, name,
                info);

            if (ext_pair) attroff(COLOR_PAIR(ext_pair));
            if (no_rd && !pair) attroff(A_DIM);
        }

        if (pair) attroff(COLOR_PAIR(pair) | A_BOLD);
    }

}

/* ---------------------------------------------------------------
   Left panel: filtered list (query active)
--------------------------------------------------------------- */
static void draw_list_filtered(const App *app) {
    for (int r = PANEL_Y; r < PANEL_Y + PANEL_H; r++)
        mvhline(r, 0, ' ', LEFT_W);

    attron(A_BOLD | A_UNDERLINE);
    mvprintw(PANEL_Y, 1, "%-*s", LEFT_W - 2, "Results");
    attroff(A_BOLD | A_UNDERLINE);

    int view_h = PANEL_H - META_PANEL_H - 1;
    int base_y = PANEL_Y + 1;

    if (app->filtered_count == 0) {
        attron(COLOR_PAIR(CP_SUGGEST));
        mvprintw(base_y, 2, "No results found.");
        if (app->suggestion[0])
            mvprintw(base_y + 1, 2, "Did you mean: \"%s\"?", app->suggestion);
        attroff(COLOR_PAIR(CP_SUGGEST));
        return;
    }

    for (int i = 0; i < view_h && i + app->list_offset < app->filtered_count; i++) {
        int fi  = app->filtered[i + app->list_offset];
        int row = base_y + i;
        int sel     = (i + app->list_offset == app->list_sel);
        int is_open = sel && (app->mode == MODE_CONTENT || app->mode == MODE_EDIT);

        int pair = sel ? (is_open ? CP_MD_HEAD : CP_SELECTED) : 0;
        if (pair) attron(COLOR_PAIR(pair) | A_BOLD);

        const char *arrow = sel ? "▶" : " ";
        const FileMeta *fm = &app->files[fi];

        char name[LEFT_W];
        strncpy(name, fm->filename, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        int name_w = LEFT_W - 3;
        if ((int)strlen(name) > name_w) name[name_w] = '\0';

        if (fm->unreadable && !pair) attron(A_DIM);
        int ext_pair = 0;
        if (!pair) {
            if (fm->binary || fm->unreadable)
                ext_pair = CP_EXT_BIN;
            else
                ext_pair = ext_color_pair(file_ext(fm->filename));
        }
        if (ext_pair) attron(COLOR_PAIR(ext_pair));

        mvprintw(row, 0, "%s %-*s", arrow, name_w, name);

        if (ext_pair) attroff(COLOR_PAIR(ext_pair));
        if (fm->unreadable && !pair) attroff(A_DIM);
        if (pair) attroff(COLOR_PAIR(pair) | A_BOLD);
    }

}

/* ---------------------------------------------------------------
   Left panel: file / dir info strip at bottom
--------------------------------------------------------------- */
static void draw_left_meta(const App *app) {
    int sep_y = PANEL_Y + PANEL_H - META_PANEL_H;

    attron(COLOR_PAIR(CP_DIVIDER));
    mvhline(sep_y, 0, ACS_HLINE, LEFT_W);
    attroff(COLOR_PAIR(CP_DIVIDER));
    for (int r = sep_y + 1; r < PANEL_Y + PANEL_H; r++)
        mvhline(r, 0, ' ', LEFT_W);

    int file_idx = -1;
    int is_dir   = 0;
    char dir_name[64] = "";
    int  dir_files    = 0;

    if (app->query[0] == '\0') {
        if (app->tree_view_count > 0) {
            int vi = app->tree_view[app->tree_sel];
            const TreeNode *n = &app->tree_all[vi];
            if (n->kind == NODE_FILE)
                file_idx = n->file_idx;
            else {
                is_dir = 1;
                strncpy(dir_name, n->name, sizeof(dir_name) - 1);
                dir_files = n->total_files;
            }
        }
    } else {
        if (app->filtered_count > 0)
            file_idx = app->filtered[app->list_sel];
    }

    if (file_idx >= 0) {
        const FileMeta *m = &app->files[file_idx];
        struct stat st;
        int have_stat = (stat(m->path, &st) == 0);

        /* Show extension color on filename */
        int fp = m->binary ? CP_EXT_BIN : ext_color_pair(file_ext(m->filename));
        if (fp) attron(COLOR_PAIR(fp) | A_BOLD); else attron(A_BOLD);
        mvprintw(sep_y + 1, 1, "%-*.*s", LEFT_W - 2, LEFT_W - 2, m->filename);
        if (fp) attroff(COLOR_PAIR(fp) | A_BOLD); else attroff(A_BOLD);

        if (m->binary) {
            attron(COLOR_PAIR(CP_EXT_BIN));
            mvprintw(sep_y + 2, 1, "[binary]");
            attroff(COLOR_PAIR(CP_EXT_BIN));
        } else if (have_stat) {
            long sz = (long)st.st_size;
            char sz_buf[12];
            if (sz >= 1024 * 1024)
                snprintf(sz_buf, sizeof(sz_buf), "%.1fM", sz / (1024.0 * 1024));
            else if (sz >= 1024)
                snprintf(sz_buf, sizeof(sz_buf), "%ldK", sz / 1024);
            else
                snprintf(sz_buf, sizeof(sz_buf), "%ldB", sz);
            mvprintw(sep_y + 2, 1, "%s  %dL", sz_buf, m->line_count);

            mode_t mo = st.st_mode;
            char perm[11];
            perm[0] = '-';
            perm[1] = (mo & S_IRUSR) ? 'r' : '-';
            perm[2] = (mo & S_IWUSR) ? 'w' : '-';
            perm[3] = (mo & S_IXUSR) ? 'x' : '-';
            perm[4] = (mo & S_IRGRP) ? 'r' : '-';
            perm[5] = (mo & S_IWGRP) ? 'w' : '-';
            perm[6] = (mo & S_IXGRP) ? 'x' : '-';
            perm[7] = (mo & S_IROTH) ? 'r' : '-';
            perm[8] = (mo & S_IWOTH) ? 'w' : '-';
            perm[9] = (mo & S_IXOTH) ? 'x' : '-';
            perm[10] = '\0';
            attron(COLOR_PAIR(CP_SUGGEST));
            mvprintw(sep_y + 3, 1, "%s", perm);
            attroff(COLOR_PAIR(CP_SUGGEST));
        } else {
            mvprintw(sep_y + 2, 1, "%dL", m->line_count);
        }
    } else if (is_dir) {
        attron(COLOR_PAIR(CP_DIR) | A_BOLD);
        mvprintw(sep_y + 1, 1, "%-*.*s", LEFT_W - 2, LEFT_W - 2, dir_name);
        attroff(COLOR_PAIR(CP_DIR) | A_BOLD);
        mvprintw(sep_y + 2, 1, "%d file%s", dir_files, dir_files == 1 ? "" : "s");
    }

    /* Library section (rows sep_y+4 .. sep_y+6) */
    int lib_sep = sep_y + 4;
    if (lib_sep < PANEL_Y + PANEL_H - 2) {
        attron(COLOR_PAIR(CP_DIVIDER));
        mvhline(lib_sep, 0, ACS_HLINE, LEFT_W);
        attroff(COLOR_PAIR(CP_DIVIDER));

        attron(COLOR_PAIR(CP_SUGGEST));
        if (app->in_lib_view)
            mvprintw(lib_sep + 1, 1, "^L  ← CWD");
        else
            mvprintw(lib_sep + 1, 1, "^L  Library");
        attroff(COLOR_PAIR(CP_SUGGEST));

        if (app->lib_dir[0]) {
            const char *sl = strrchr(app->lib_dir, '/');
            const char *base = sl ? sl + 1 : app->lib_dir;
            mvprintw(lib_sep + 2, 1, "%-*.*s", LEFT_W - 2, LEFT_W - 2, base);
        } else {
            attron(A_DIM);
            mvprintw(lib_sep + 2, 1, "not found");
            attroff(A_DIM);
        }
    }
}

/* Dispatcher */
static void draw_list(const App *app) {
    if (app->query[0] == '\0')
        draw_list_tree(app);
    else
        draw_list_filtered(app);
}

/* ---------------------------------------------------------------
   Notes panel helpers
--------------------------------------------------------------- */

static void rebuild_notes_view(App *app) {
    app->notes_view_count = 0;
    NoteVault *v = app->vault;
    if (!v) return;

    for (int ci = 0; ci < v->cat_count; ci++) {
        if (app->notes_view_count >= NOTE_MAX_NOTES + NOTE_MAX_CATS) break;
        app->notes_view[app->notes_view_count++] = (NViewItem){ NVIEW_CAT, ci };
        for (int ni = 0; ni < v->note_count; ni++) {
            if (strcmp(v->notes[ni].cat_id, v->cats[ci].id) != 0) continue;
            if (app->notes_view_count >= NOTE_MAX_NOTES + NOTE_MAX_CATS) break;
            app->notes_view[app->notes_view_count++] = (NViewItem){ NVIEW_NOTE, ni };
        }
    }
    /* Notes with no matching category */
    for (int ni = 0; ni < v->note_count; ni++) {
        int found = 0;
        for (int ci = 0; ci < v->cat_count && !found; ci++)
            if (strcmp(v->notes[ni].cat_id, v->cats[ci].id) == 0) found = 1;
        if (!found) {
            if (app->notes_view_count >= NOTE_MAX_NOTES + NOTE_MAX_CATS) break;
            app->notes_view[app->notes_view_count++] = (NViewItem){ NVIEW_NOTE, ni };
        }
    }

    if (app->notes_sel >= app->notes_view_count)
        app->notes_sel = app->notes_view_count > 0 ? app->notes_view_count - 1 : 0;
}

/* Mini file list drawn in the bottom half of the left panel when notes are open. */
static void draw_mini_file_list(const App *app, int start_y, int avail_h) {
    for (int r = start_y; r < start_y + avail_h; r++)
        mvhline(r, 0, ' ', LEFT_W);

    attron(A_BOLD | A_DIM);
    mvprintw(start_y, 1, "%-*s", LEFT_W - 2, "Files");
    attroff(A_BOLD | A_DIM);

    int list_h = avail_h - 1;
    int base_y = start_y + 1;

    for (int i = 0; i < list_h && i + app->tree_offset < app->tree_view_count; i++) {
        int vi           = app->tree_view[i + app->tree_offset];
        const TreeNode *n = &app->tree_all[vi];
        int row    = base_y + i;
        int is_sel = (i + app->tree_offset == app->tree_sel);
        int is_open = is_sel && (app->open_file >= 0);

        int pair = 0;
        if      (is_sel && !is_open)  pair = CP_SELECTED;
        else if (is_open)             pair = CP_MD_HEAD;
        else if (n->kind == NODE_DIR) pair = CP_DIR;
        if (pair) attron(COLOR_PAIR(pair) | A_BOLD);

        int avail = LEFT_W - 2 - n->depth * 2 - 1;
        if (avail < 2) avail = 2;
        char name[64];
        strncpy(name, n->name, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        if ((int)strlen(name) > avail) name[avail] = '\0';

        if (n->kind == NODE_DIR)
            mvprintw(row, 0, " %*s[%c]%-*s",
                     n->depth * 2, "", n->expanded ? '-' : '+',
                     avail - 3 < 1 ? 1 : avail - 3, name);
        else
            mvprintw(row, 0, " %*s%-*s",
                     n->depth * 2, "", avail, name);

        if (pair) attroff(COLOR_PAIR(pair) | A_BOLD);
    }
}

static void draw_notes_panel(const App *app) {
    int notes_h = PANEL_H / 2;   /* rows dedicated to notes (including header+hint) */

    /* Clear notes section */
    for (int r = PANEL_Y; r < PANEL_Y + notes_h; r++)
        mvhline(r, 0, ' ', LEFT_W);

    /* Notes header */
    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvprintw(PANEL_Y, 0, " Notes ");
    attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

    NoteVault *v = app->vault;
    int view_h = notes_h - 2;   /* -1 header, -1 hint */

    if (!v || app->notes_view_count == 0) {
        attron(A_DIM);
        mvprintw(PANEL_Y + 2, 1, "Ctrl+N ile not oluştur");
        attroff(A_DIM);
    } else {
        int offset = app->notes_offset;
        for (int vi = offset;
             vi < app->notes_view_count && vi - offset < view_h; vi++) {
            int row    = PANEL_Y + 1 + (vi - offset);
            int is_sel = (vi == app->notes_sel);
            NViewItem item = app->notes_view[vi];

            if (is_sel) attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);

            if (item.kind == NVIEW_CAT) {
                NoteCat *c = &v->cats[item.idx];
                if (!is_sel) attron(COLOR_PAIR(CP_DIR) | A_BOLD);
                mvprintw(row, 0, "%-*.*s", LEFT_W, LEFT_W, c->name);
                if (!is_sel) attroff(COLOR_PAIR(CP_DIR) | A_BOLD);
            } else {
                Note *n = &v->notes[item.idx];
                int title_w = LEFT_W - 8;
                const char *type_tag = (n->type == NOTE_TODO) ? "[todo]" : "[text]";
                const char *lock = n->has_pw ? "*" : " ";
                mvprintw(row, 0, " %s%-*.*s%s",
                         lock, title_w, title_w, n->title, type_tag);
            }

            if (is_sel) attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
        }
    }

    /* Notes hint */
    attron(COLOR_PAIR(CP_SUGGEST));
    mvprintw(PANEL_Y + notes_h - 1, 0, "^N ^D ^/  ^Y←Files");
    attroff(COLOR_PAIR(CP_SUGGEST));

    /* Separator */
    attron(A_DIM);
    mvhline(PANEL_Y + notes_h, 0, ACS_HLINE, LEFT_W);
    attroff(A_DIM);

    /* File list in bottom half */
    draw_mini_file_list(app, PANEL_Y + notes_h + 1, PANEL_H - notes_h - 1);
}

/* Load note content into app->content (for display/edit).
   Requires note to already be decrypted (n->content != NULL). */
static void load_note_content(App *app, int note_idx) {
    NoteVault *v = app->vault;
    if (!v || note_idx < 0 || note_idx >= v->note_count) return;
    Note *n = &v->notes[note_idx];
    if (!n->content) return;

    content_free(&app->content);
    bat_free(app);

    /* Split by newlines */
    const char *src = n->content;
    int cap = 32;
    app->content.lines = malloc(cap * sizeof(char *));
    if (!app->content.lines) return;
    app->content.line_count = 0;
    app->content.capacity   = cap;

    const char *p = src;
    for (;;) {
        const char *nl = strchr(p, '\n');
        size_t ll = nl ? (size_t)(nl - p) : strlen(p);
        if (app->content.line_count >= app->content.capacity) {
            int nc = app->content.capacity * 2;
            char **nlines = realloc(app->content.lines, nc * sizeof(char *));
            if (!nlines) break;
            app->content.lines    = nlines;
            app->content.capacity = nc;
        }
        char *line = malloc(ll + 1);
        if (!line) break;
        memcpy(line, p, ll);
        line[ll] = '\0';
        app->content.lines[app->content.line_count++] = line;
        if (!nl) break;
        p = nl + 1;
    }

    if (app->content.line_count == 0) {
        /* Empty note: at least one empty line so editor works */
        app->content.lines[0] = strdup("");
        app->content.line_count = 1;
    }

    app->open_note  = note_idx;
    app->open_file  = -1;
    app->content_offset = 0;
    app->content_dirty  = 0;
}

/* Join app->content back to a single string and encrypt into vault */
static int save_note(App *app) {
    if (!app->vault || app->open_note < 0) return 0;
    NoteVault *v = app->vault;
    Note *n = &v->notes[app->open_note];

    size_t total = 0;
    for (int i = 0; i < app->content.line_count; i++)
        total += strlen(app->content.lines[i]) + 1;

    char *text = malloc(total + 1);
    if (!text) return 0;

    size_t pos = 0;
    for (int i = 0; i < app->content.line_count; i++) {
        size_t ll = strlen(app->content.lines[i]);
        memcpy(text + pos, app->content.lines[i], ll);
        pos += ll;
        if (i < app->content.line_count - 1) text[pos++] = '\n';
    }
    text[pos] = '\0';

    if (n->content) {
        sodium_memzero(n->content, strlen(n->content));
        free(n->content);
    }
    n->content = text;

    const char *pw = app->note_active_pw[0] ? app->note_active_pw : NULL;
    vault_encrypt_note(v, app->open_note, pw);
    vault_save(v);
    app->content_dirty = 0;
    return 1;
}

/* Vertical divider
--------------------------------------------------------------- */
static int rpanel_h(const App *app) {
    return app->term_panel_open ? PANEL_H - TERM_PANEL_H - 1 : PANEL_H;
}

static void draw_divider(void) {
    attron(COLOR_PAIR(CP_DIVIDER));
    for (int r = PANEL_Y; r < PANEL_Y + PANEL_H; r++)
        mvaddch(r, LEFT_W, ACS_VLINE);
    attroff(COLOR_PAIR(CP_DIVIDER));
}

/* ---------------------------------------------------------------
   Highlight all occurrences of app->highlight_word in a drawn row.
   col_start = screen column where the text begins.
   shown / shown_len = the already-truncated string that was printed.
--------------------------------------------------------------- */
static void highlight_occurrences(const App *app, int row, int col_start,
                                   const char *shown, int shown_len) {
    if (!app->highlight_word[0]) return;
    int nlen = (int)strlen(app->highlight_word);
    int i    = 0;
    while (i + nlen <= shown_len) {
        int off;
        if (ci_find(shown + i, app->highlight_word, &off)) {
            mvchgat(row, col_start + i + off, nlen, A_REVERSE, 0, NULL);
            i += off + nlen;
        } else {
            break;
        }
    }
}

/* ---------------------------------------------------------------
   bat integration
--------------------------------------------------------------- */
static const char *bat_find(void) {
    static char path[256] = "";
    if (path[0]) return path[0] == '-' ? NULL : path;
    const char *c[] = {
        "/opt/homebrew/bin/bat", "/usr/local/bin/bat", "/usr/bin/bat", NULL
    };
    for (int i = 0; c[i]; i++) {
        if (access(c[i], X_OK) == 0) { strncpy(path, c[i], 255); return path; }
    }
    path[0] = '-';
    return NULL;
}

static short bat_rgb_to_pair(int r, int g, int b) {
    typedef struct { int r,g,b; short cp; } C;
    static const C t[] = {
        {249, 38, 114, CP_BAT_KW},
        {102,217, 239, CP_BAT_TYPE},
        {166,226,  46, CP_BAT_DEFN},
        {230,219, 116, CP_BAT_STR},
        {190,132, 255, CP_BAT_NUM},
        {253,151,  31, CP_BAT_PARAM},
        {117,113,  94, CP_BAT_CMT},
    };
    /* skip near-white (normal text) and very dark */
    int lum = r*77 + g*150 + b*29;
    if (lum > 54000 || lum < 4000) return 0;
    int best = INT_MAX; short cp = 0;
    for (int i = 0; i < 7; i++) {
        int d = (r-t[i].r)*(r-t[i].r)+(g-t[i].g)*(g-t[i].g)+(b-t[i].b)*(b-t[i].b);
        if (d < best) { best = d; cp = t[i].cp; }
    }
    return cp;
}

/* Parse a bat output line (with ANSI codes) → span array.
   Also fills text[] (ANSI-stripped). Caller frees returned array. */
static HLSpan *bat_parse_line(const char *bat, char *text, int tsz, int *nout) {
    int cap = 64;
    HLSpan *spans = malloc(cap * sizeof(HLSpan));
    int n = 0, col = 0, span_start = 0;
    short cur_cp = 0; attr_t cur_at = 0;
    int tlen = 0;
    const char *p = bat;

    while (*p) {
        if ((unsigned char)*p != 0x1b) {
            if (tlen < tsz-1) text[tlen++] = *p;
            col++; p++;
            continue;
        }
        /* ESC — flush current span */
        if (col > span_start && cur_cp) {
            if (n >= cap) { cap *= 2; spans = realloc(spans, cap*sizeof(HLSpan)); }
            spans[n++] = (HLSpan){ span_start, col-span_start, cur_cp, cur_at };
        }
        span_start = col;
        p++;  /* skip ESC */

        if (*p == '[') {
            /* CSI sequence — collect numeric params, skip to final byte (0x40–0x7e) */
            p++;
            int pv[8], np = 0;
            while (*p && !(*p >= 0x40 && *p <= 0x7e)) {
                if (*p >= '0' && *p <= '9') {
                    int v = 0;
                    while (*p >= '0' && *p <= '9') v = v*10 + (*p++ - '0');
                    if (np < 8) pv[np++] = v;
                } else {
                    p++;  /* skip ';', '?', '!', '>' and other parameter bytes */
                }
            }
            char final = *p;
            if (*p) p++;

            if (final == 'm') {
                /* SGR — scan all params; handles compound sequences like 0;38;2;R;G;B */
                cur_cp = 0; cur_at = 0;
                for (int i = 0; i < np; i++) {
                    if (pv[i] == 1) { cur_at |= A_BOLD; }
                    else if (pv[i] == 3) { cur_at |= A_ITALIC; }
                    else if (pv[i] == 38 && i+1 < np && pv[i+1] == 2 && i+4 < np) {
                        cur_cp = bat_rgb_to_pair(pv[i+2], pv[i+3], pv[i+4]);
                        break;
                    } else if (pv[i] == 38 && i+1 < np && pv[i+1] == 5 && i+2 < np) {
                        int idx = pv[i+2];
                        if (idx >= 1 && idx <= 9) cur_cp = CP_BAT_KW;
                        else if (idx == 14 || idx == 87) cur_cp = CP_BAT_TYPE;
                        break;
                    }
                }
            }
            /* non-SGR CSI (erase, cursor movement, etc.) — already skipped, no text change */
        } else if (*p == ']') {
            /* OSC sequence — skip until BEL (0x07) or ST (ESC \) */
            p++;
            while (*p) {
                if ((unsigned char)*p == 0x07) { p++; break; }
                if ((unsigned char)*p == 0x1b && p[1] == '\\') { p += 2; break; }
                p++;
            }
        } else if (*p) {
            p++;  /* other 2-char ESC sequence — skip the one byte after ESC */
        }
    }
    text[tlen] = '\0';
    /* flush tail */
    if (col > span_start && cur_cp) {
        if (n >= cap) { cap *= 2; spans = realloc(spans, cap*sizeof(HLSpan)); }
        spans[n++] = (HLSpan){ span_start, col-span_start, cur_cp, cur_at };
    }
    *nout = n;
    return spans;
}

void bat_free(App *app) {
    if (!app->bat_rows) return;
    for (int i = 0; i < app->bat_nrows; i++) free(app->bat_rows[i]);
    free(app->bat_rows);
    free(app->bat_row_sizes);
    app->bat_rows = NULL; app->bat_row_sizes = NULL; app->bat_nrows = 0;
}

/* Render line[cs..cs+clen) using pre-computed bat spans */
static void hl_render_line(int y, int x, const char *line, int cs, int clen,
                            const HLSpan *spans, int nspans) {
    int ce = cs + clen, pos = cs;
    for (int si = 0; si < nspans; si++) {
        int sf = spans[si].col, se = sf + spans[si].len;
        if (se <= cs) continue;
        if (sf >= ce) break;
        /* gap before span */
        int gf = pos, gt = sf < ce ? sf : ce;
        if (gf < gt) mvaddnstr(y, x+(gf-cs), line+gf, gt-gf);
        /* span */
        int df = sf > cs ? sf : cs, dt = se < ce ? se : ce;
        if (df < dt) {
            if (spans[si].cp) attron(COLOR_PAIR(spans[si].cp)|spans[si].attr);
            else if (spans[si].attr) attron(spans[si].attr);
            mvaddnstr(y, x+(df-cs), line+df, dt-df);
            if (spans[si].cp) attroff(COLOR_PAIR(spans[si].cp)|spans[si].attr);
            else if (spans[si].attr) attroff(spans[si].attr);
        }
        pos = se > pos ? se : pos;
    }
    if (pos < ce) mvaddnstr(y, x+(pos-cs), line+pos, ce-pos);
}

/* Wrap src in single-quotes, escaping embedded ' as '\''.
   Safe for passing arbitrary paths to POSIX shells via popen(). */
static void shell_quote(const char *src, char *out, size_t outsz) {
    size_t i = 0;
    if (outsz < 3) { if (outsz) out[0] = '\0'; return; }
    out[i++] = '\'';
    for (; *src && i < outsz - 4; src++) {
        if (*src == '\'') {
            out[i++] = '\''; out[i++] = '\\';
            out[i++] = '\''; out[i++] = '\'';
        } else {
            out[i++] = *src;
        }
    }
    out[i++] = '\'';
    out[i]   = '\0';
}

/* Load file via bat, filling content + bat_rows. Returns 1 on success. */
static int load_content_bat_impl(App *app, int file_idx) {
    const char *bat = bat_find();
    if (!bat) return 0;

    const FileMeta *m = &app->files[file_idx];
    char qpath[META_PATH_LEN * 4 + 4];
    shell_quote(m->path, qpath, sizeof(qpath));
    char cmd[META_PATH_LEN * 4 + 128];
    snprintf(cmd, sizeof(cmd),
             "%s --color=always --style=plain --paging=never "
             "--terminal-width=4096 %s 2>/dev/null", bat, qpath);

    FILE *f = popen(cmd, "r");
    if (!f) return 0;

    int cap = 256;
    char **lines     = malloc(cap * sizeof(char *));
    HLSpan **rows    = malloc(cap * sizeof(HLSpan *));
    int     *rsizes  = malloc(cap * sizeof(int));
    if (!lines || !rows || !rsizes) { pclose(f); free(lines); free(rows); free(rsizes); return 0; }

    char bat_line[8192], text[4096];
    int n = 0;
    while (fgets(bat_line, sizeof(bat_line), f)) {
        bat_line[strcspn(bat_line, "\r\n")] = '\0';
        int nspans = 0;
        HLSpan *spans = bat_parse_line(bat_line, text, sizeof(text), &nspans);
        if (n >= cap) {
            cap *= 2;
            lines  = realloc(lines,  cap * sizeof(char *));
            rows   = realloc(rows,   cap * sizeof(HLSpan *));
            rsizes = realloc(rsizes, cap * sizeof(int));
        }
        lines[n]  = strdup(text);
        rows[n]   = spans;
        rsizes[n] = nspans;
        n++;
    }
    pclose(f);

    if (n == 0) { free(lines); free(rows); free(rsizes); return 0; }

    app->content.lines      = lines;
    app->content.line_count = n;
    app->content.capacity   = cap;
    app->bat_rows      = rows;
    app->bat_row_sizes = rsizes;
    app->bat_nrows     = n;
    return 1;
}

/* ---------------------------------------------------------------
   Syntax highlighting engine (fallback when bat not available)
--------------------------------------------------------------- */
typedef enum {
    LANG_NONE, LANG_C, LANG_PY, LANG_PHP, LANG_JS, LANG_SH, LANG_GO, LANG_RS
} Lang;

static Lang detect_lang(const char *ext) {
    if (!ext || !ext[0]) return LANG_NONE;
    if (strcmp(ext,"c")==0   || strcmp(ext,"h")==0   || strcmp(ext,"cpp")==0 ||
        strcmp(ext,"cc")==0  || strcmp(ext,"cxx")==0 || strcmp(ext,"hpp")==0) return LANG_C;
    if (strcmp(ext,"py")==0)  return LANG_PY;
    if (strcmp(ext,"php")==0) return LANG_PHP;
    if (strcmp(ext,"js")==0  || strcmp(ext,"ts")==0  ||
        strcmp(ext,"jsx")==0 || strcmp(ext,"tsx")==0) return LANG_JS;
    if (strcmp(ext,"sh")==0  || strcmp(ext,"bash")==0 ||
        strcmp(ext,"zsh")==0 || strcmp(ext,"fish")==0) return LANG_SH;
    if (strcmp(ext,"go")==0)  return LANG_GO;
    if (strcmp(ext,"rs")==0)  return LANG_RS;
    return LANG_NONE;
}

#define KW(s) (wlen==(int)(sizeof(s)-1) && memcmp(w,(s),wlen)==0)

static int is_ctrl_kw(const char *w, int wlen, Lang lang) {
    /* Common control flow */
    if (KW("if")||KW("else")||KW("for")||KW("while")||KW("do")||
        KW("switch")||KW("case")||KW("break")||KW("continue")||
        KW("return")||KW("goto")||KW("default")||
        KW("try")||KW("catch")||KW("finally")||KW("throw")||KW("throws")) return 1;
    /* Access/storage modifiers (all langs) */
    if (KW("static")||KW("public")||KW("private")||KW("protected")||
        KW("abstract")||KW("final")||KW("readonly")||KW("override")||
        KW("virtual")||KW("explicit")||KW("friend")||KW("mutable")) return 1;
    if (lang==LANG_PY&&(KW("elif")||KW("pass")||KW("raise")||KW("with")||
        KW("try")||KW("except")||KW("finally")||KW("yield")||
        KW("and")||KW("or")||KW("not")||KW("in")||KW("is")||
        KW("as")||KW("lambda")||KW("del")||KW("assert")||KW("async")||KW("await"))) return 1;
    if ((lang==LANG_JS||lang==LANG_PHP)&&(KW("try")||KW("catch")||
        KW("finally")||KW("throw")||KW("typeof")||KW("instanceof")||
        KW("in")||KW("of")||KW("yield")||KW("await")||KW("async"))) return 1;
    if (lang==LANG_PHP&&KW("match")) return 1;
    if (lang==LANG_GO&&(KW("range")||KW("defer")||KW("go")||
        KW("select")||KW("fallthrough"))) return 1;
    if (lang==LANG_RS&&(KW("loop")||KW("match")||KW("move")||
        KW("where")||KW("unsafe")||KW("dyn")||KW("ref")||KW("in"))) return 1;
    return 0;
}

static int is_def_kw(const char *w, int wlen, Lang lang) {
    if (KW("function")) return 1;            /* PHP, JS */
    if (lang==LANG_PY && KW("def"))   return 1;
    if (lang==LANG_GO && KW("func"))  return 1;
    if (lang==LANG_RS && KW("fn"))    return 1;
    if (lang==LANG_C  && KW("__attribute__")) return 1;
    return 0;
}

static int is_builtin_kw(const char *w, int wlen, Lang lang) {
    if (lang==LANG_PHP&&(KW("echo")||KW("print")||KW("isset")||
        KW("empty")||KW("unset")||KW("die")||KW("exit"))) return 1;
    if (lang==LANG_PY&&(KW("print")||KW("len")||KW("range")||
        KW("input")||KW("open")||KW("type")||KW("isinstance")||
        KW("hasattr")||KW("getattr")||KW("setattr")||KW("repr")||
        KW("str")||KW("int")||KW("float")||KW("list")||KW("dict")||
        KW("tuple")||KW("set")||KW("bool")||KW("bytes"))) return 1;
    return 0;
}

static int is_type_kw(const char *w, int wlen, Lang lang) {
    /* C/C++ primitive types and constants */
    if (KW("int")||KW("char")||KW("void")||KW("float")||KW("double")||
        KW("long")||KW("short")||KW("unsigned")||KW("signed")||
        KW("const")||KW("extern")||KW("inline")||
        KW("volatile")||KW("register")||KW("auto")||KW("struct")||
        KW("union")||KW("enum")||KW("typedef")||KW("bool")||
        KW("true")||KW("false")||KW("NULL")||KW("nullptr")||
        KW("TRUE")||KW("FALSE")||KW("sizeof")||KW("typeof")||
        KW("size_t")||KW("ssize_t")||KW("uint8_t")||KW("uint16_t")||
        KW("uint32_t")||KW("uint64_t")||KW("int8_t")||KW("int16_t")||
        KW("int32_t")||KW("int64_t")||KW("ptrdiff_t")||KW("uintptr_t")||
        KW("wchar_t")||KW("FILE")||KW("_Bool")) return 1;
    if (lang==LANG_PY&&(KW("None")||KW("True")||KW("False")||
        KW("class")||KW("import")||KW("from")||KW("global")||
        KW("nonlocal")||KW("super")||KW("self"))) return 1;
    if (lang==LANG_PHP&&(KW("null")||KW("true")||KW("false")||
        KW("class")||KW("interface")||KW("trait")||KW("extends")||
        KW("implements")||KW("namespace")||KW("use")||
        KW("new")||KW("string")||KW("array")||KW("int")||KW("float")||
        KW("object")||KW("mixed")||KW("self")||KW("parent")||KW("this"))) return 1;
    if (lang==LANG_JS&&(KW("null")||KW("undefined")||KW("true")||
        KW("false")||KW("NaN")||KW("class")||KW("extends")||
        KW("interface")||KW("type")||KW("const")||KW("let")||KW("var")||
        KW("import")||KW("export")||KW("from")||
        KW("string")||KW("number")||KW("boolean")||KW("any")||
        KW("never")||KW("unknown")||KW("void")||
        KW("new")||KW("this")||KW("super"))) return 1;
    if (lang==LANG_GO&&(KW("var")||KW("const")||KW("type")||
        KW("struct")||KW("interface")||KW("map")||KW("chan")||
        KW("nil")||KW("true")||KW("false")||KW("string")||
        KW("int")||KW("int8")||KW("int16")||KW("int32")||KW("int64")||
        KW("uint")||KW("uint8")||KW("uint16")||KW("uint32")||KW("uint64")||
        KW("float32")||KW("float64")||KW("bool")||KW("byte")||
        KW("rune")||KW("error")||KW("package")||KW("import"))) return 1;
    if (lang==LANG_RS&&(KW("let")||KW("mut")||KW("pub")||
        KW("impl")||KW("use")||KW("mod")||KW("struct")||KW("enum")||
        KW("trait")||KW("type")||KW("const")||KW("extern")||
        KW("true")||KW("false")||KW("Some")||KW("None")||KW("Ok")||
        KW("Err")||KW("Box")||KW("Vec")||KW("String")||KW("str")||
        KW("i32")||KW("i64")||KW("u32")||KW("u64")||KW("f32")||KW("f64")||
        KW("usize")||KW("isize")||KW("bool")||KW("char")||
        KW("Self")||KW("self")||KW("super")||KW("crate"))) return 1;
    return 0;
}

#undef KW

/* Emit line[from..to) at screen (y, x+(from-cs)), clipped to [cs,ce).
   cp=0 means no color pair (use attr only). */
static void syn_emit(int y, int x, const char *line, int cs, int ce,
                     int from, int to, int cp, attr_t at) {
    int df = from > cs ? from : cs;
    int dt = to   < ce ? to   : ce;
    if (df >= dt) return;
    int n = dt - df;
    if (cp) attron(COLOR_PAIR(cp)|at); else if (at) attron(at);
    mvaddnstr(y, x + (df - cs), line + df, n);
    if (cp) attroff(COLOR_PAIR(cp)|at); else if (at) attroff(at);
}

/* Render line[cs..cs+clen) at (y,x) with syntax colors.
   line_start_ml: 1 if we entered this line inside a multiline comment.
   Returns end-of-line multiline-comment state (for next line). */
static int render_syntax_chunk(int y, int x, int mw __attribute__((unused)),
                                const char *line, int cs, int clen,
                                int line_start_ml, Lang lang) {
    int full = (int)strlen(line);
    int ce   = cs + clen;
    if (ce > full) ce = full;
    int in_ml    = line_start_ml;
    int after_def = 0;   /* 1 after function/def/fn → next ident is def name */
    int i = 0;

    /* Detect preprocessor line (C/PHP) */
    int is_preproc = 0;
    if (lang == LANG_C || lang == LANG_PHP) {
        int ti = 0;
        while (ti < full && (line[ti]==' '||line[ti]=='\t')) ti++;
        if (ti < full && line[ti] == '#') is_preproc = 1;
    }

    while (i < full) {

        /* ── Inside multiline comment ── */
        if (in_ml) {
            int start = i;
            while (i < full) {
                if (line[i]=='*' && i+1<full && line[i+1]=='/') { i+=2; in_ml=0; break; }
                i++;
            }
            syn_emit(y, x, line, cs, ce, start, i, CP_BAT_CMT, A_DIM);
            continue;
        }

        unsigned char c = (unsigned char)line[i];

        /* ── Line comment ── */
        if (line[i]=='/' && i+1<full && line[i+1]=='/') {
            syn_emit(y, x, line, cs, ce, i, full, CP_BAT_CMT, A_DIM);
            i = full; continue;
        }
        if ((lang==LANG_PY||lang==LANG_SH) && line[i]=='#' && !is_preproc) {
            syn_emit(y, x, line, cs, ce, i, full, CP_BAT_CMT, A_DIM);
            i = full; continue;
        }
        /* PHP also allows # as line comment */
        if (lang==LANG_PHP && line[i]=='#' && !is_preproc) {
            syn_emit(y, x, line, cs, ce, i, full, CP_BAT_CMT, A_DIM);
            i = full; continue;
        }

        /* ── Multiline comment start ── */
        if (line[i]=='/' && i+1<full && line[i+1]=='*') {
            int start = i; i += 2; in_ml = 1;
            while (i < full) {
                if (line[i]=='*' && i+1<full && line[i+1]=='/') { i+=2; in_ml=0; break; }
                i++;
            }
            syn_emit(y, x, line, cs, ce, start, i, CP_BAT_CMT, A_DIM);
            continue;
        }

        /* ── Preprocessor line ── */
        if (is_preproc && i == 0) {
            /* find start of directive word */
            int j = 0;
            while (j < full && (line[j]==' '||line[j]=='\t')) j++;
            j++; /* skip # */
            while (j < full && isalpha((unsigned char)line[j])) j++;
            /* # + keyword in cyan bold */
            syn_emit(y, x, line, cs, ce, 0, j, CP_BAT_KW, A_BOLD);
            i = j;
            /* skip whitespace */
            while (i < full && (line[i]==' '||line[i]=='\t')) {
                syn_emit(y, x, line, cs, ce, i, i+1, 0, 0);
                i++;
            }
            /* include argument in string color */
            if (i < full && (line[i]=='"'||line[i]=='<')) {
                char close = line[i]=='<' ? '>' : '"';
                int qend = i+1;
                while (qend < full && line[qend] != close) qend++;
                if (qend < full) qend++;
                syn_emit(y, x, line, cs, ce, i, qend, CP_BAT_STR, 0);
                i = qend;
            }
            /* rest of preprocessor line (e.g. macro body) — no color */
            continue;
        }

        /* ── String double-quote ── */
        if (line[i] == '"') {
            int start = i; i++;
            while (i < full && line[i] != '"') {
                if (line[i]=='\\') i++;
                i++;
            }
            if (i < full) i++;
            syn_emit(y, x, line, cs, ce, start, i, CP_BAT_STR, 0);
            continue;
        }

        /* ── String single-quote (char literal / PHP/Python string) ── */
        if (line[i] == '\'') {
            int start = i; i++;
            while (i < full && line[i] != '\'') {
                if (line[i]=='\\') i++;
                i++;
            }
            if (i < full) i++;
            syn_emit(y, x, line, cs, ce, start, i, CP_BAT_STR, 0);
            continue;
        }

        /* ── Number ── */
        if (isdigit(c) || (line[i]=='.' && i+1<full && isdigit((unsigned char)line[i+1]))) {
            int start = i;
            if (line[i]=='0' && i+1<full && (line[i+1]=='x'||line[i+1]=='X')) {
                i += 2;
                while (i < full && isxdigit((unsigned char)line[i])) i++;
            } else if (line[i]=='0' && i+1<full && (line[i+1]=='b'||line[i+1]=='B')) {
                i += 2;
                while (i < full && (line[i]=='0'||line[i]=='1')) i++;
            } else {
                while (i < full && (isdigit((unsigned char)line[i]) ||
                       line[i]=='.' || line[i]=='e' || line[i]=='E' ||
                       line[i]=='f' || line[i]=='F' || line[i]=='l' ||
                       line[i]=='L' || line[i]=='u' || line[i]=='U')) i++;
            }
            syn_emit(y, x, line, cs, ce, start, i, CP_BAT_NUM, 0);
            continue;
        }

        /* ── Identifier / keyword / function name ── */
        if (isalpha(c) || line[i]=='_' || (lang==LANG_PHP && line[i]=='$')) {
            int start = i;
            if (line[i]=='$') i++; /* PHP variable $ prefix */
            while (i < full && (isalnum((unsigned char)line[i]) || line[i]=='_')) i++;
            int wlen = i - start;

            /* Look back for -> arrow (method call) */
            int lns = 0;
            {
                int k = start - 1;
                while (k >= 0 && (line[k]==' '||line[k]=='\t')) k--;
                if (k >= 0) lns = (unsigned char)line[k];
            }
            int after_arrow = (lns == '>');

            /* peek past spaces for '(' */
            int j = i;
            while (j < full && line[j]==' ') j++;
            int is_func = (j < full && line[j]=='(');

            const char *w = line + start;
            int cp = 0; attr_t at = 0;

            if (after_def) {
                cp = CP_BAT_DEFN; at = 0; after_def = 0;
            } else if (after_arrow) {
                cp = CP_BAT_PARAM; at = A_BOLD;
            } else if (is_def_kw(w, wlen, lang)) {
                cp = CP_BAT_PARAM; at = A_BOLD; after_def = 1;
            } else if (is_ctrl_kw(w, wlen, lang)) {
                cp = CP_BAT_KW; at = A_BOLD;
            } else if (is_builtin_kw(w, wlen, lang)) {
                cp = CP_BAT_PARAM; at = 0;
            } else if (is_type_kw(w, wlen, lang)) {
                cp = CP_BAT_TYPE; at = 0;
            } else if (is_func) {
                cp = CP_BAT_PARAM; at = A_BOLD;
            }

            syn_emit(y, x, line, cs, ce, start, i, cp, at);
            continue;
        }

        /* ── Default: single char ── */
        syn_emit(y, x, line, cs, ce, i, i+1, 0, 0);
        i++;
    }

    return in_ml;
}

/* ---------------------------------------------------------------
   Right panel: content viewer
--------------------------------------------------------------- */
static void draw_content(const App *app) {
    int rph = rpanel_h(app);
    for (int r = PANEL_Y; r < PANEL_Y + rph; r++)
        mvhline(r, RIGHT_X, ' ', RIGHT_W);

    if (app->open_file < 0 && app->open_note < 0) {
        /* In notes mode show a short hint instead of the full index */
        if (app->mode == MODE_NOTES || app->mode == MODE_NOTE_PW ||
            app->mode == MODE_NOTE_NEW || app->mode == MODE_NOTE_NEW_CAT ||
            app->mode == MODE_NOTE_SEARCH) {
            int clip = COLS - RIGHT_X - 3;
            int y = PANEL_Y + 2;
            attron(A_DIM);
            mvprintw(y++, RIGHT_X + 2, "%-*s", clip, "  ↑ ↓    not seç");
            mvprintw(y++, RIGHT_X + 2, "%-*s", clip, "  →      not aç");
            mvprintw(y++, RIGHT_X + 2, "%-*s", clip, "  Ctrl+N  yeni not");
            attroff(A_DIM);
            return;
        }

        const char *msg[] = {
            "",
            "  cref  —  C Reference Browser",
            "",
            "  Navigate:",
            "  • ↑ ↓          : move up/down in list",
            "  • → / Enter    : open file / expand dir",
            "  • ←            : collapse dir / back",
            "  • /            : search by tag/title/category",
            "  • [ ]          : jump between sections",
            "  • g / G        : top / bottom of file",
            "",
            "  Controls:",
            "  • Ctrl+Y            : toggle notes panel",
            "  • Ctrl+B            : build (make / gcc)",
            "  • Ctrl+O            : toggle terminal panel",
            "  • Ctrl+L            : toggle CWD ↔ library",
            "  • Ctrl+T            : filter by file type",
            "",
            "  Editor  (press e to enter, Esc to exit):",
            "  • Ctrl+S            : save",
            "  • Ctrl+Z            : undo",
            "  • Ctrl+A            : select all",
            "  • Ctrl+C/X/V        : copy / cut / paste",
            "  • Ctrl+L            : select current line",
            "  • Ctrl+D            : duplicate current line",
            "  • Ctrl+K            : delete current line",
            "  • Ctrl+F / Ctrl+G   : find / find next",
            "  • Shift+←→↑↓        : extend selection",
            "  • Ctrl+←→           : word jump",
            "  • Ctrl+Shift+←→     : word selection",
            "  • Fn+←→             : line start / end (Mac)",
            "  • Shift+Fn+←→       : select to line start / end",
            "",
            "  Press ? or F1 for full help (scrollable).",
            "",
            "  CLI flags:",
            "  • cref <query>           pre-fill search",
            "  • cref -d <dir>          set directory",
            "  • cref --filetype md,txt extra file types",
            "  • cref --grep <word>     search in files",
            "  • cref --score <word>    match count",
            NULL
        };
        int clip = COLS - RIGHT_X - 3;
        attron(COLOR_PAIR(CP_MD_HEAD) | A_BOLD);
        print_clipped(PANEL_Y + 1, RIGHT_X + 2, clip, msg[1]);
        attroff(COLOR_PAIR(CP_MD_HEAD) | A_BOLD);
        for (int i = 2; msg[i]; i++)
            print_clipped(PANEL_Y + i, RIGHT_X + 2, clip, msg[i]);
        return;
    }

    const FileMeta *m = (app->open_file >= 0) ? &app->files[app->open_file] : NULL;

    /* Note header (when a note is open instead of a file) */
    if (app->open_note >= 0 && app->vault) {
        Note *n = &app->vault->notes[app->open_note];
        attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
        mvhline(PANEL_Y, RIGHT_X, ' ', RIGHT_W);
        {
            char title_buf[300];
            snprintf(title_buf, sizeof(title_buf), "%s%s%s%s",
                n->has_pw ? "* " : "",
                n->title,
                app->content_dirty ? " *" : "",
                app->mode == MODE_EDIT ? "  [EDIT]" : "");
            mvprintw(PANEL_Y, RIGHT_X + 1, "%-*s", RIGHT_W - 2, title_buf);
        }
        attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);
        mvhline(PANEL_Y + 1, RIGHT_X, ' ', RIGHT_W); /* no tag row for notes */
        goto draw_lines;
    }

    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvhline(PANEL_Y, RIGHT_X, ' ', RIGHT_W);
    {
        char title_buf[280];
        snprintf(title_buf, sizeof(title_buf), "%s%s%s",
            m->title,
            app->content_dirty ? " *" : "",
            app->mode == MODE_EDIT ? "  [EDIT]" : "");
        mvprintw(PANEL_Y, RIGHT_X + 1, "%-*s", RIGHT_W - 2, title_buf);
    }
    attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

    if (m->tag_count > 0 && PANEL_H > 3) {
        int ex = RIGHT_X + 1;
        mvhline(PANEL_Y + 1, RIGHT_X, ' ', RIGHT_W);
        attron(COLOR_PAIR(CP_TAG));
        for (int i = 0; i < m->tag_count && ex < RIGHT_X + RIGHT_W - 10; i++) {
            int tw = strlen(m->tags[i]) + 3;
            if (ex + tw < COLS - 1) {
                mvprintw(PANEL_Y + 1, ex, "[%s]", m->tags[i]);
                ex += tw + 1;
            }
        }
        attroff(COLOR_PAIR(CP_TAG));
    }

    draw_lines: ;
    int base_row  = PANEL_Y + 2;
    int view_h    = rph - 2;
    int in_code   = 0;
    int lnum_col  = RIGHT_X + 1;
    int cont_col  = RIGHT_X + 1 + LNUM_W;
    int max_w     = RIGHT_W - 2 - LNUM_W;
    if (max_w < 1) max_w = 1;

    int mw = max_w < 511 ? max_w : 511;

    /* Syntax highlighting state */
    Lang lang      = m ? detect_lang(file_ext(m->filename)) : LANG_NONE;
    int  syn_state = 0;   /* 0=normal, 1=inside multiline comment */

    int di = 0;                      /* current display row index */
    int si = app->content_offset;    /* current logical line index */

    while (di < view_h && si < app->content.line_count) {
        const char *line = app->content.lines[si];
        int llen = (int)strlen(line);

        /* bat spans: skip in edit mode so the built-in tokenizer handles live edits */
        int use_bat = (app->mode != MODE_EDIT && app->bat_nrows > 0 && si < app->bat_nrows);
        /* Markdown-specific flags (skip for bat/source files) */
        int is_src = (!use_bat && lang != LANG_NONE);
        int is_fence   = (!use_bat && !is_src && strncmp(line, "```", 3) == 0);
        int is_divider = (!use_bat && !is_src && !is_fence && !in_code &&
                          (strncmp(line, "---", 3) == 0 || strncmp(line, "===", 3) == 0));
        if (is_fence) in_code = !in_code;
        int is_heading = (!use_bat && !is_src && !is_fence && !in_code && line[0] == '#');
        int is_coded   = (!use_bat && !is_src && (is_fence || in_code));

        /* Line number on first display row of this logical line */
        int row = base_row + di;
        attron(COLOR_PAIR(CP_SUGGEST));
        mvprintw(row, lnum_col, "%4d| ", si + 1);
        attroff(COLOR_PAIR(CP_SUGGEST));

        if (is_divider) {
            attron(COLOR_PAIR(CP_DIVIDER));
            mvhline(row, cont_col, ACS_HLINE, mw);
            attroff(COLOR_PAIR(CP_DIVIDER));
            di++; si++;
            continue;
        }

        int num_chunks = (llen == 0) ? 1 : (llen + mw - 1) / mw;
        int line_ml    = syn_state;   /* ML-comment state at start of this line */
        int last_ml    = line_ml;

        for (int chunk = 0; chunk < num_chunks && di < view_h; chunk++) {
            row = base_row + di;

            if (chunk > 0) {
                /* Continuation rows get a blank line-number gutter */
                attron(COLOR_PAIR(CP_SUGGEST));
                mvprintw(row, lnum_col, "      ");
                attroff(COLOR_PAIR(CP_SUGGEST));
            }

            char shown[512];
            int cs = chunk * mw;
            int avail = llen - cs;
            int copy  = avail < mw ? avail : mw;
            if (copy < 0) copy = 0;
            strncpy(shown, line + cs, copy);
            shown[copy] = '\0';
            int slen = (int)strlen(shown);

            if (use_bat) {
                hl_render_line(row, cont_col, line, cs, copy,
                               app->bat_rows[si], app->bat_row_sizes[si]);
                highlight_occurrences(app, row, cont_col, shown, slen);
            } else if (is_src) {
                last_ml = render_syntax_chunk(row, cont_col, mw,
                                              line, cs, copy, line_ml, lang);
                highlight_occurrences(app, row, cont_col, shown, slen);
            } else if (is_coded) {
                attron(COLOR_PAIR(CP_CODE));
                mvprintw(row, cont_col, "%-*s", mw, shown);
                attroff(COLOR_PAIR(CP_CODE));
                if (!is_fence)
                    highlight_occurrences(app, row, cont_col, shown, slen);
            } else if (is_heading) {
                attron(COLOR_PAIR(CP_MD_HEAD) | A_BOLD);
                mvprintw(row, cont_col, "%-*s", mw, shown);
                attroff(COLOR_PAIR(CP_MD_HEAD) | A_BOLD);
                highlight_occurrences(app, row, cont_col, shown, slen);
            } else {
                mvprintw(row, cont_col, "%-*s", mw, shown);
                highlight_occurrences(app, row, cont_col, shown, slen);
            }

            di++;
        }
        if (is_src) syn_state = last_ml;
        si++;
    }

    /* Second pass: find match overlay (A_UNDERLINE).
       Drawn before selection so selection (A_REVERSE) wins visually when both
       overlap. */
    if (app->find_query_len > 0) {
        int qlen = app->find_query_len;
        int fdi  = 0;
        for (int fsi = app->content_offset;
             fsi < app->content.line_count && fdi < view_h;
             fsi++) {
            const char *fl = app->content.lines[fsi];
            int fllen      = (int)strlen(fl);
            int fc         = (fllen == 0) ? 1 : (fllen + mw - 1) / mw;
            /* find all matches in this line */
            int from = 0, m;
            while ((m = find_in_line_from(fl, app->find_query, from)) >= 0) {
                int m_end = m + qlen;
                /* paint match across possibly multiple wrap chunks */
                for (int chunk = 0; chunk < fc && fdi + chunk < view_h; chunk++) {
                    int cs = chunk * mw;
                    int ce = cs + mw;
                    if (m_end <= cs || m >= ce) continue;
                    int hl_from = m > cs ? m - cs : 0;
                    int hl_to   = (m_end < ce ? m_end : ce) - cs;
                    int sw      = hl_to - hl_from;
                    if (sw > 0 && hl_from < mw) {
                        /* if this is the cursor's current match, use stronger
                           attribute so the active match stands out */
                        int is_current = (fsi == app->cursor_row &&
                                           m == app->cursor_col);
                        attr_t at = is_current
                            ? (A_REVERSE | A_BOLD)
                            : (A_UNDERLINE | A_BOLD);
                        mvchgat(base_row + fdi + chunk, cont_col + hl_from,
                                sw > mw - hl_from ? mw - hl_from : sw,
                                at, 0, NULL);
                    }
                }
                from = m_end;
            }
            fdi += fc;
        }
    }

    /* Third pass: selection overlay (always wins over find highlight).
       Iterates by (logical-line, chunk) so it correctly covers wrapped rows,
       headings, code blocks, and dividers. */
    if (app->sel_active) {
        int r0, c0, r1, c1;
        sel_normalize(app, &r0, &c0, &r1, &c1);
        int sdi = 0;
        for (int ssi = app->content_offset;
             ssi < app->content.line_count && sdi < view_h;
             ssi++) {
            if (ssi > r1) break;
            const char *sl = app->content.lines[ssi];
            int sllen      = (int)strlen(sl);
            int sc         = (sllen == 0) ? 1 : (sllen + mw - 1) / mw;
            for (int chunk = 0; chunk < sc && sdi < view_h; chunk++, sdi++) {
                if (ssi < r0) continue;
                int cs         = chunk * mw;
                int ce         = cs + mw;
                int sel_from   = (ssi == r0) ? c0 : 0;
                int sel_to     = (ssi == r1) ? c1 : sllen;
                if (sel_from >= ce || sel_to <= cs) continue;
                int hl_from = sel_from > cs ? sel_from - cs : 0;
                int hl_to   = (sel_to < ce ? sel_to : ce) - cs;
                int sw      = hl_to - hl_from;
                if (sw > 0 && hl_from < mw)
                    mvchgat(base_row + sdi, cont_col + hl_from,
                            sw > mw - hl_from ? mw - hl_from : sw,
                            A_REVERSE, 0, NULL);
            }
        }
    }

    if (app->content.line_count > 0) {
        int pct = (app->content_offset + view_h) * 100 /
                   app->content.line_count;
        if (pct > 100) pct = 100;
        attron(COLOR_PAIR(CP_TAG) | A_BOLD);
        mvprintw(PANEL_Y, COLS - 9, " [%3d%%]", pct);
        attroff(COLOR_PAIR(CP_TAG) | A_BOLD);
    }

}

/* ---------------------------------------------------------------
   Grep results panel (full-screen)
--------------------------------------------------------------- */
static void draw_grep(const App *app) {
    for (int r = PANEL_Y; r < PANEL_Y + PANEL_H; r++)
        mvhline(r, 0, ' ', COLS);

    attron(A_BOLD | A_UNDERLINE);
    mvprintw(PANEL_Y, 1, "grep: \"%s\"  —  %d hit%s%s",
        app->grep_query,
        app->grep_hit_count,
        app->grep_hit_count == 1 ? "" : "s",
        app->grep_hit_count >= GREP_MAX_HITS ? "  (capped)" : "");
    attroff(A_BOLD | A_UNDERLINE);

    if (app->grep_hit_count == 0) {
        attron(COLOR_PAIR(CP_SUGGEST));
        mvprintw(PANEL_Y + 2, 2, "No content matches for \"%s\".", app->grep_query);
        attroff(COLOR_PAIR(CP_SUGGEST));
        return;
    }

    int view_h  = PANEL_H - 2;
    int base_y  = PANEL_Y + 1;
    int cont_w  = COLS - GREP_CONT_X - 1;
    if (cont_w < 1) cont_w = 1;

    for (int i = 0; i < view_h && i + app->grep_offset < app->grep_hit_count; i++) {
        int hi           = i + app->grep_offset;
        const GrepHit *hit = &app->grep_hits[hi];
        int row = base_y + i;
        int sel = (hi == app->grep_sel);

        char fn[20];
        strncpy(fn, app->files[hit->file_idx].filename, sizeof(fn) - 1);
        fn[sizeof(fn) - 1] = '\0';
        int fnl = strlen(fn);
        if (fnl > 3 && strcmp(fn + fnl - 3, ".md") == 0) fn[fnl - 3] = '\0';

        char label[GREP_LBL_W + 8];
        snprintf(label, sizeof(label), "%s:%d", fn, hit->line_no);

        int mc    = hit->match_col;
        int ml    = hit->match_len;
        int start = 0;
        if (mc >= cont_w) {
            start = mc - cont_w / 3;
            if (start < 0) start = 0;
        }

        mvprintw(row, 0, "%s %-*s │ %-*.*s",
            sel ? "▶" : " ",
            GREP_LBL_W, label,
            cont_w, cont_w, hit->line + start);

        if (sel) {
            mvchgat(row, 0, -1, A_BOLD, CP_SELECTED, NULL);
        } else {
            int mx = GREP_CONT_X + (mc - start);
            if (mx >= GREP_CONT_X && mx < COLS - 1 && ml > 0) {
                int mw = ml;
                if (mx + mw > COLS - 1) mw = COLS - 1 - mx;
                mvchgat(row, mx, mw, A_BOLD, CP_TAG, NULL);
            }
        }
    }

    if (app->grep_hit_count > view_h) {
        attron(COLOR_PAIR(CP_TAG));
        mvprintw(PANEL_Y + PANEL_H - 1, 1,
            "  ↑↓ %d/%d", app->grep_sel + 1, app->grep_hit_count);
        attroff(COLOR_PAIR(CP_TAG));
    }
}

/* ---------------------------------------------------------------
   Compile output panel (right side)
--------------------------------------------------------------- */
static void draw_compile_out(const App *app) {
    int rph = rpanel_h(app);
    for (int r = PANEL_Y; r < PANEL_Y + rph; r++)
        mvhline(r, RIGHT_X, ' ', RIGHT_W);

    const char *hdr = app->compile_ok ? "Build Output  [OK]" : "Build Output  [FAILED]";
    if (app->compile_ok)
        attron(A_BOLD | A_UNDERLINE | COLOR_PAIR(CP_BAT_DEFN));
    else
        attron(A_BOLD | A_UNDERLINE | COLOR_PAIR(CP_EXT_BIN));
    mvprintw(PANEL_Y, RIGHT_X + 2, "%-*s", RIGHT_W - 3, hdr);
    attroff(A_BOLD | A_UNDERLINE | COLOR_PAIR(CP_BAT_DEFN) | COLOR_PAIR(CP_EXT_BIN));

    int view_h = rph - 2;
    int total  = app->compile_out.line_count;
    int off    = app->compile_out_off;

    for (int i = 0; i < view_h && off + i < total; i++) {
        int row         = PANEL_Y + 1 + i;
        const char *ln  = app->compile_out.lines[off + i];
        if (strstr(ln, "error:") || strstr(ln, ": error")) {
            attron(COLOR_PAIR(CP_EXT_BIN) | A_BOLD);
            mvprintw(row, RIGHT_X + 1, "%-*.*s", RIGHT_W - 2, RIGHT_W - 2, ln);
            attroff(COLOR_PAIR(CP_EXT_BIN) | A_BOLD);
        } else if (strstr(ln, "warning:") || strstr(ln, ": warning")) {
            attron(COLOR_PAIR(CP_BAT_STR) | A_BOLD);
            mvprintw(row, RIGHT_X + 1, "%-*.*s", RIGHT_W - 2, RIGHT_W - 2, ln);
            attroff(COLOR_PAIR(CP_BAT_STR) | A_BOLD);
        } else {
            mvprintw(row, RIGHT_X + 1, "%-*.*s", RIGHT_W - 2, RIGHT_W - 2, ln);
        }
    }
    if (total == 0) {
        attron(A_DIM);
        mvprintw(PANEL_Y + 2, RIGHT_X + 2, "(no output)");
        attroff(A_DIM);
    }
}

/* ---------------------------------------------------------------
   Full redraw
--------------------------------------------------------------- */
static void draw_help(const App *app);      /* forward declaration */
static void draw_term_panel(const App *app);
static void toggle_term(App *app);

static void redraw(const App *app) {
    erase();
    if (app->mode == MODE_HELP) {
        draw_help(app);
        return;
    }
    draw_header(app);
    if (app->mode == MODE_GREP) {
        draw_grep(app);
    } else if (app->mode == MODE_COMPILE_OUT) {
        draw_list(app);
        draw_left_meta(app);
        draw_divider();
        draw_compile_out(app);
    } else if (app->mode == MODE_NOTES || app->mode == MODE_NOTE_PW ||
               app->mode == MODE_NOTE_NEW || app->mode == MODE_NOTE_NEW_CAT ||
               app->mode == MODE_NOTE_SEARCH) {
        draw_notes_panel(app);
        draw_divider();
        draw_content(app);
        /* Overlay prompts on the right panel */
        if (app->mode == MODE_NOTE_PW) {
            int mid = PANEL_Y + PANEL_H / 2;
            attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
            mvhline(mid,     RIGHT_X + 1, ' ', RIGHT_W - 2);
            mvhline(mid + 1, RIGHT_X + 1, ' ', RIGHT_W - 2);
            mvhline(mid + 2, RIGHT_X + 1, ' ', RIGHT_W - 2);
            mvprintw(mid,     RIGHT_X + 2, "Note password:");
            attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
            /* Show masked or error hint */
            int pw_x = RIGHT_X + 2 + 15;
            for (int i = 0; i < app->note_pw_len; i++)
                mvaddch(mid + 1, pw_x + i, '*');
            mvaddch(mid + 1, pw_x + app->note_pw_len, '_');
            mvprintw(mid + 2, RIGHT_X + 2, "[Enter] unlock  [Esc] cancel");
        } else if (app->mode == MODE_NOTE_NEW) {
            int mid = PANEL_Y + PANEL_H / 2;
            attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
            mvhline(mid,     RIGHT_X + 1, ' ', RIGHT_W - 2);
            mvhline(mid + 1, RIGHT_X + 1, ' ', RIGHT_W - 2);
            mvhline(mid + 2, RIGHT_X + 1, ' ', RIGHT_W - 2);
            if (app->note_new_step == 0) {
                mvprintw(mid, RIGHT_X + 2, "Yeni not basligi:");
                attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
                mvprintw(mid + 1, RIGHT_X + 2, "%s", app->note_new_title);
                mvprintw(mid + 2, RIGHT_X + 2, "[Enter] ileri  [Esc] iptal");
                move(mid + 1, RIGHT_X + 2 + app->note_new_title_len);
            } else if (app->note_new_step == 1) {
                mvprintw(mid, RIGHT_X + 2, "Sifre eklensin mi? [y/n]");
                attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
                mvprintw(mid + 1, RIGHT_X + 2, "Not: \"%s\"", app->note_new_title);
                mvprintw(mid + 2, RIGHT_X + 2, "[Esc] iptal");
            } else if (app->note_new_step == 2) {
                mvprintw(mid, RIGHT_X + 2, "Yeni sifre:");
                attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
                for (int i = 0; i < app->note_new_pw1_len; i++)
                    mvaddch(mid + 1, RIGHT_X + 2 + i, '*');
                mvaddch(mid + 1, RIGHT_X + 2 + app->note_new_pw1_len, '_');
                mvprintw(mid + 2, RIGHT_X + 2, "[Enter] ileri  [Esc] iptal");
                move(mid + 1, RIGHT_X + 2 + app->note_new_pw1_len);
            } else { /* step 3 */
                mvprintw(mid, RIGHT_X + 2, "Sifre onay:");
                attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
                for (int i = 0; i < app->note_new_pw2_len; i++)
                    mvaddch(mid + 1, RIGHT_X + 2 + i, '*');
                mvaddch(mid + 1, RIGHT_X + 2 + app->note_new_pw2_len, '_');
                mvprintw(mid + 2, RIGHT_X + 2, "[Enter] olustur  [Esc] iptal");
                move(mid + 1, RIGHT_X + 2 + app->note_new_pw2_len);
            }
        } else if (app->mode == MODE_NOTE_SEARCH) {
            /* Search bar at top of right panel, hits below */
            attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
            mvhline(PANEL_Y, RIGHT_X, ' ', RIGHT_W);
            mvprintw(PANEL_Y, RIGHT_X + 1, " Search notes: %s_", app->note_sq);
            attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
            int row = PANEL_Y + 1;
            for (int i = app->note_hit_offset;
                 i < app->note_hit_count && row < PANEL_Y + PANEL_H - 1; i++, row++) {
                const NoteHit *h = &app->note_hits[i];
                int is_sel = (i == app->note_hit_sel);
                if (is_sel) attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
                const char *title = app->vault ? app->vault->notes[h->note_idx].title : "";
                mvprintw(row, RIGHT_X + 1, "%-14.14s  L%-4d  %-*.*s",
                         title, h->line_no,
                         RIGHT_W - 26, RIGHT_W - 26, h->line);
                if (is_sel) attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
            }
        } else if (app->mode == MODE_NOTE_NEW_CAT) {
            int mid = PANEL_Y + PANEL_H / 2;
            attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
            mvhline(mid,     RIGHT_X + 1, ' ', RIGHT_W - 2);
            mvhline(mid + 1, RIGHT_X + 1, ' ', RIGHT_W - 2);
            mvhline(mid + 2, RIGHT_X + 1, ' ', RIGHT_W - 2);
            mvprintw(mid, RIGHT_X + 2, "Yeni klasor adi:");
            attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
            mvprintw(mid + 1, RIGHT_X + 2, "%s", app->note_new_cat_buf);
            mvprintw(mid + 2, RIGHT_X + 2, "[Enter] olustur  [Esc] iptal");
            move(mid + 1, RIGHT_X + 2 + app->note_new_cat_len);
        }
    } else if ((app->mode == MODE_EDIT || app->mode == MODE_NOTE_SET_PW)
               && app->open_note >= 0) {
        draw_notes_panel(app);
        draw_divider();
        draw_content(app);
        /* Overlay for Ctrl+E password change */
        if (app->mode == MODE_NOTE_SET_PW) {
            int mid = PANEL_Y + PANEL_H / 2;
            attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
            mvhline(mid,     RIGHT_X + 1, ' ', RIGHT_W - 2);
            mvhline(mid + 1, RIGHT_X + 1, ' ', RIGHT_W - 2);
            mvhline(mid + 2, RIGHT_X + 1, ' ', RIGHT_W - 2);
            mvhline(mid + 3, RIGHT_X + 1, ' ', RIGHT_W - 2);
            const char *prompt =
                app->note_setpw_step == 0 ? "Mevcut sifre:" :
                app->note_setpw_step == 1 ? "Yeni sifre (bos = kaldir):" :
                                            "Sifre onay:";
            mvprintw(mid, RIGHT_X + 2, "%s", prompt);
            attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
            /* masked input */
            int cur_len = app->note_setpw_step == 0 ? app->note_setpw_old_len
                        : app->note_setpw_step == 1 ? app->note_setpw_new1_len
                        :                              app->note_setpw_new2_len;
            for (int i = 0; i < cur_len; i++)
                mvaddch(mid + 1, RIGHT_X + 2 + i, '*');
            mvaddch(mid + 1, RIGHT_X + 2 + cur_len, '_');
            if (app->note_setpw_msg[0])
                mvprintw(mid + 2, RIGHT_X + 2, "%s", app->note_setpw_msg);
            else
                mvprintw(mid + 2, RIGHT_X + 2, "[Enter] onayla  [Esc] iptal");
            move(mid + 1, RIGHT_X + 2 + cur_len);
        }
    } else {
        draw_list(app);
        draw_left_meta(app);
        draw_divider();
        draw_content(app);
    }
    if (app->term_panel_open)
        draw_term_panel(app);
    draw_status(app);

    /* Cursor positioning must be the very last thing before refresh(),
       otherwise any subsequent mvprintw call will move it away. */
    if (app->mode == MODE_EDIT_FIND) {
        /* place cursor on the find prompt in the status bar */
        move(STATUS_Y, 8 + app->find_query_len);
    } else if (app->mode == MODE_EDIT && (app->open_file >= 0 || app->open_note >= 0)) {
        int base_row = PANEL_Y + 2;
        int cont_col = RIGHT_X + 1 + LNUM_W;
        int view_h   = rpanel_h(app) - 2;
        int wmw      = RIGHT_W - 2 - LNUM_W; if (wmw < 1) wmw = 1;
        /* Count display rows consumed by lines before cursor_row */
        int di = 0;
        for (int s = app->content_offset; s < app->cursor_row && di < view_h; s++) {
            int ll = (int)strlen(app->content.lines[s]);
            di += (ll == 0) ? 1 : (ll + wmw - 1) / wmw;
        }
        /* Wrapped row index and display column (UTF-8 aware) */
        const char *cline = app->content.lines[app->cursor_row];
        int chunk        = app->cursor_col / wmw;
        int byte_in_chunk = app->cursor_col - chunk * wmw;
        int cr = base_row + di + chunk;
        int cc = cont_col + utf8_display_col(cline + chunk * wmw, byte_in_chunk);
        if (cr >= base_row && cr < base_row + view_h)
            move(cr, cc);
    } else if (app->mode == MODE_TERM) {
        if (app->term && app->term->alive) {
            int term_y = PANEL_Y + PANEL_H - TERM_PANEL_H;
            move(term_y + app->term->crow, RIGHT_X + app->term->ccol);
        }
    }

    refresh();
}

/* ---------------------------------------------------------------
   Update filtered list from current query
--------------------------------------------------------------- */
static void update_search(App *app) {
    app->suggestion[0] = '\0';
    app->filtered_count = 0;

    if (!app->files || app->file_count == 0 || !app->filtered) {
        app->list_sel    = 0;
        app->list_offset = 0;
        return;
    }

    app->filtered_count = search(app->files, app->file_count,
                                 app->query,
                                 app->filtered, app->file_count);

    if (app->filtered_count == 0 && app->query[0])
        find_suggestion(app->files, app->file_count,
                        app->query, app->suggestion, sizeof(app->suggestion));

    app->list_sel    = 0;
    app->list_offset = 0;
}

/* ---------------------------------------------------------------
   Load file content into buffer
--------------------------------------------------------------- */
static void load_content(App *app, int file_idx) {
    content_free(&app->content);
    bat_free(app);

    if (app->files[file_idx].unreadable) {
        int cap = 2;
        app->content.lines    = malloc(cap * sizeof(char *));
        if (!app->content.lines) return;
        app->content.lines[0] = strdup("[Permission denied]");
        app->content.line_count = 1;
        app->content.capacity   = cap;
        app->open_file          = file_idx;
        app->content_offset     = 0;
        app->mode               = MODE_CONTENT;
        return;
    }

    if (app->files[file_idx].binary) {
        int cap = 2;
        app->content.lines    = malloc(cap * sizeof(char *));
        if (!app->content.lines) return;
        app->content.lines[0] = strdup("[Binary file — cannot display]");
        app->content.line_count = 1;
        app->content.capacity   = cap;
        app->open_file          = file_idx;
        app->content_offset     = 0;
        app->mode               = MODE_CONTENT;
        return;
    }

    /* bat opt-in: set CREF_USE_BAT=1 to enable (built-in tokenizer is used by default) */
    if (getenv("CREF_USE_BAT") && load_content_bat_impl(app, file_idx)) {
        app->open_file      = file_idx;
        app->content_offset = 0;
        return;
    }

    FILE *f = fopen(app->files[file_idx].path, "r");
    if (!f) return;

    int cap = 128;
    app->content.lines = malloc(cap * sizeof(char *));
    if (!app->content.lines) { fclose(f); return; }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';

        if (app->content.line_count >= cap) {
            cap *= 2;
            char **tmp = realloc(app->content.lines, cap * sizeof(char *));
            if (!tmp) break;
            app->content.lines = tmp;
        }
        app->content.lines[app->content.line_count++] = strdup(line);
    }
    /* Empty file: ensure at least one empty line so editors can place cursor */
    if (app->content.line_count == 0) {
        app->content.lines[0] = strdup("");
        app->content.line_count = 1;
    }
    app->content.capacity = cap;

    fclose(f);
    app->open_file      = file_idx;
    app->content_offset = 0;
}

/* ---------------------------------------------------------------
   Editor: cursor helpers
--------------------------------------------------------------- */
static void cursor_scroll(App *app) {
    int view_h = rpanel_h(app) - 2;
    if (app->cursor_row < app->content_offset)
        app->content_offset = app->cursor_row;
    /* Scroll down: backward scan from cursor_row to find the minimum
       content_offset where cursor_row fits within view_h display rows.
       This accounts for wrapped lines — the simple formula
       (cursor_row - view_h + 1) can leave cursor off-screen when lines
       between content_offset and cursor_row consume extra display rows. */
    {
        int mw = COLS - (LEFT_W + 1) - 2 - LNUM_W;
        if (mw < 1) mw = 1;
        int new_off = app->cursor_row;
        int disp = 0;
        for (int i = app->cursor_row; i >= 0; i--) {
            int llen = (int)strlen(app->content.lines[i]);
            int nc = (llen == 0) ? 1 : (llen + mw - 1) / mw;
            disp += nc;
            if (disp > view_h) { new_off = i + 1; break; }
            new_off = i;
        }
        if (app->content_offset < new_off)
            app->content_offset = new_off;
    }
    if (app->content_offset < 0) app->content_offset = 0;
}

static void cursor_clamp(App *app) {
    if (app->content.line_count == 0) { app->cursor_row = 0; app->cursor_col = 0; return; }
    if (app->cursor_row < 0) app->cursor_row = 0;
    if (app->cursor_row >= app->content.line_count)
        app->cursor_row = app->content.line_count - 1;
    int linelen = (int)strlen(app->content.lines[app->cursor_row]);
    if (app->cursor_col > linelen) app->cursor_col = linelen;
    if (app->cursor_col < 0) app->cursor_col = 0;
}

static void cursor_move_up(App *app) {
    if (app->cursor_row > 0) { app->cursor_row--; cursor_clamp(app); cursor_scroll(app); }
}
static void cursor_move_down(App *app) {
    if (app->cursor_row < app->content.line_count - 1) {
        app->cursor_row++; cursor_clamp(app); cursor_scroll(app);
    }
}
static void cursor_move_left(App *app) {
    if (app->cursor_col > 0) {
        const char *s = app->content.lines[app->cursor_row];
        int col = app->cursor_col - 1;
        /* skip back over UTF-8 continuation bytes */
        while (col > 0 && ((unsigned char)s[col] & 0xC0) == 0x80) col--;
        app->cursor_col = col;
    } else if (app->cursor_row > 0) {
        app->cursor_row--;
        app->cursor_col = (int)strlen(app->content.lines[app->cursor_row]);
        cursor_scroll(app);
    }
}
static void cursor_move_right(App *app) {
    const char *s = app->content.lines[app->cursor_row];
    int linelen = (int)strlen(s);
    if (app->cursor_col < linelen) {
        app->cursor_col += utf8_char_bytes((unsigned char)s[app->cursor_col]);
        if (app->cursor_col > linelen) app->cursor_col = linelen;
    } else if (app->cursor_row < app->content.line_count - 1) {
        app->cursor_row++; app->cursor_col = 0; cursor_scroll(app);
    }
}

/* Mac-style word boundaries: groups separated by spaces or character-class changes
   (letter/digit ↔ punctuation). Matches Option+Arrow behavior on macOS. */
static int word_boundary_back(const char *s, int col) {
    int i = col;
    while (i > 0 && s[i-1] == ' ') i--;      /* skip trailing spaces */
    if (i == 0) return 0;
    int cls = isalnum((unsigned char)s[i-1]); /* class of last non-space char */
    while (i > 0 && s[i-1] != ' ' &&
           (int)isalnum((unsigned char)s[i-1]) == cls) i--;
    return i;
}
static int word_boundary_fwd(const char *s, int col) {
    int len = (int)strlen(s), i = col;
    if (i >= len) return len;
    int cls = isalnum((unsigned char)s[i]);   /* class of current char */
    while (i < len && s[i] != ' ' &&
           (int)isalnum((unsigned char)s[i]) == cls) i++;
    while (i < len && s[i] == ' ') i++;       /* skip following spaces */
    return i;
}

/* ---------------------------------------------------------------
   Editor: selection helpers
--------------------------------------------------------------- */
static void sel_clear(App *app) { app->sel_active = 0; }

static void sel_start(App *app) {
    app->sel_anchor_row = app->cursor_row;
    app->sel_anchor_col = app->cursor_col;
    app->sel_active     = 1;
}

static void sel_normalize(const App *app,
                           int *r0, int *c0, int *r1, int *c1) {
    int ar = app->sel_anchor_row, ac = app->sel_anchor_col;
    int cr = app->cursor_row,    cc = app->cursor_col;
    if (ar < cr || (ar == cr && ac <= cc)) {
        *r0 = ar; *c0 = ac; *r1 = cr; *c1 = cc;
    } else {
        *r0 = cr; *c0 = cc; *r1 = ar; *c1 = ac;
    }
}

/* ---------------------------------------------------------------
   Editor: undo stack
--------------------------------------------------------------- */
static void undo_evict(App *app) {
    if (app->undo_stack[0].text) { free(app->undo_stack[0].text); }
    memmove(app->undo_stack, app->undo_stack + 1,
            (UNDO_CAP - 1) * sizeof(UndoEntry));
    app->undo_top--;
    if (app->undo_top < 0) app->undo_top = 0;
    app->undo_count--;
}

static UndoEntry *undo_alloc(App *app) {
    if (app->undo_count >= UNDO_CAP) undo_evict(app);
    UndoEntry *e = &app->undo_stack[app->undo_top++];
    memset(e, 0, sizeof(*e));
    app->undo_count++;
    return e;
}

/* ---------------------------------------------------------------
   Editor: line buffer helpers (used by undo/edit ops)
--------------------------------------------------------------- */
static void content_insert_line(App *app, int at, const char *text) {
    if (app->content.line_count >= app->content.capacity) {
        int cap = app->content.capacity * 2;
        char **tmp = realloc(app->content.lines, cap * sizeof(char *));
        if (!tmp) return;
        app->content.lines    = tmp;
        app->content.capacity = cap;
    }
    memmove(&app->content.lines[at + 1],
            &app->content.lines[at],
            (app->content.line_count - at) * sizeof(char *));
    app->content.lines[at] = strdup(text ? text : "");
    app->content.line_count++;
}

static void content_delete_line(App *app, int at) {
    if (at < 0 || at >= app->content.line_count) return;
    free(app->content.lines[at]);
    memmove(&app->content.lines[at],
            &app->content.lines[at + 1],
            (app->content.line_count - at - 1) * sizeof(char *));
    app->content.line_count--;
}

/* Replace a single line's text. */
static void line_set(App *app, int row, const char *text) {
    free(app->content.lines[row]);
    app->content.lines[row] = strdup(text ? text : "");
}

/* Insert `ch` into line at byte position col. */
static void line_insert_char(App *app, int row, int col, char ch) {
    const char *old = app->content.lines[row];
    int len = (int)strlen(old);
    char *neu = malloc(len + 2);
    if (!neu) return;
    memcpy(neu, old, col);
    neu[col] = ch;
    memcpy(neu + col + 1, old + col, len - col + 1);
    free(app->content.lines[row]);
    app->content.lines[row] = neu;
}

/* Insert n bytes at byte position col (for multi-byte UTF-8 characters). */
static void line_insert_bytes(App *app, int row, int col, const char *bytes, int n) {
    const char *old = app->content.lines[row];
    int olen = (int)strlen(old);
    char *neu = malloc(olen + n + 1);
    if (!neu) return;
    memcpy(neu, old, col);
    memcpy(neu + col, bytes, n);
    memcpy(neu + col + n, old + col, olen - col + 1);
    free(app->content.lines[row]);
    app->content.lines[row] = neu;
}

/* Delete byte at position col from line. */
static void line_delete_char(App *app, int row, int col) {
    char *s = app->content.lines[row];
    int len = (int)strlen(s);
    if (col < 0 || col >= len) return;
    memmove(s + col, s + col + 1, len - col);
}

/* ---------------------------------------------------------------
   Editor: undo apply (pop top entry and reverse it)
--------------------------------------------------------------- */
static void undo_pop(App *app) {
    if (app->undo_count <= 0) return;
    app->undo_top--;
    app->undo_count--;
    UndoEntry *e = &app->undo_stack[app->undo_top];

    switch (e->kind) {
    case UNDO_CHAR_INS:
        {
            int nb = e->col2 > 0 ? e->col2 : 1;
            for (int i = 0; i < nb; i++)
                line_delete_char(app, e->row, e->col);
        }
        app->cursor_row = e->row;
        app->cursor_col = e->col;
        break;

    case UNDO_CHAR_DEL:
        if (e->text) {
            int nb = (int)strlen(e->text);
            line_insert_bytes(app, e->row, e->col, e->text, nb);
            app->cursor_col = e->col + nb;
            free(e->text); e->text = NULL;
        } else {
            app->cursor_col = e->col + 1;
        }
        app->cursor_row = e->row;
        break;

    case UNDO_SPLIT:
        /* Two lines at e->row and e->row+1 → join them back */
        if (e->row + 1 < app->content.line_count) {
            const char *a = app->content.lines[e->row];
            const char *b = app->content.lines[e->row + 1];
            int la = (int)strlen(a), lb = (int)strlen(b);
            char *joined = malloc(la + lb + 1);
            if (joined) {
                memcpy(joined, a, la);
                memcpy(joined + la, b, lb + 1);
                free(app->content.lines[e->row]);
                app->content.lines[e->row] = joined;
                content_delete_line(app, e->row + 1);
            }
        }
        app->cursor_row = e->row;
        app->cursor_col = e->col;
        break;

    case UNDO_JOIN:
        /* Lines were joined into e->row at col e->col → re-split */
        {
            const char *full = app->content.lines[e->row];
            int sp = e->col;
            int fl = (int)strlen(full);
            char *first = malloc(sp + 1);
            char *rest  = malloc(fl - sp + 1);
            if (first && rest) {
                memcpy(first, full, sp); first[sp] = '\0';
                memcpy(rest, full + sp, fl - sp + 1);
                line_set(app, e->row, first);
                content_insert_line(app, e->row + 1, rest);
            }
            free(first); free(rest);
        }
        app->cursor_row = e->row + 1;
        app->cursor_col = 0;
        break;

    case UNDO_BLOCK_DEL:
        /* Re-insert the deleted block at (e->row, e->col) */
        if (e->text) {
            /* Split text on '\n' and insert lines */
            int r = e->row;
            const char *orig_line = app->content.lines[r];
            int  ol = (int)strlen(orig_line);
            int  c  = e->col;

            /* Split text into lines */
            char *block = strdup(e->text);
            if (!block) { free(e->text); e->text = NULL; break; }
            int blines = 1;
            for (char *p = block; *p; p++) if (*p == '\n') blines++;

            char **segs = malloc(blines * sizeof(char *));
            if (!segs) { free(block); free(e->text); e->text = NULL; break; }
            int si = 0; char *tok = block, *nl;
            while ((nl = strchr(tok, '\n')) != NULL) {
                *nl = '\0'; segs[si++] = tok; tok = nl + 1;
            }
            segs[si] = tok;

            if (blines == 1) {
                /* single-line re-insert */
                int slen = (int)strlen(segs[0]);
                char *neu = malloc(ol + slen + 1);
                if (neu) {
                    memcpy(neu, orig_line, c);
                    memcpy(neu + c, segs[0], slen);
                    memcpy(neu + c + slen, orig_line + c, ol - c + 1);
                    line_set(app, r, neu); free(neu);
                }
            } else {
                /* multi-line: split current line, insert segments */
                char *tail = strdup(orig_line + c);
                char *head = malloc(c + strlen(segs[0]) + 1);
                if (head && tail) {
                    memcpy(head, orig_line, c);
                    strcpy(head + c, segs[0]);
                    line_set(app, r, head);
                    for (int li = 1; li < blines - 1; li++)
                        content_insert_line(app, r + li, segs[li]);
                    char *last = malloc(strlen(segs[blines-1]) + strlen(tail) + 1);
                    if (last) {
                        strcpy(last, segs[blines - 1]);
                        strcat(last, tail);
                        content_insert_line(app, r + blines - 1, last);
                        free(last);
                    }
                }
                free(head); free(tail);
            }
            free(segs); free(block);
            free(e->text); e->text = NULL;
        }
        app->cursor_row = e->row;
        app->cursor_col = e->col;
        break;

    case UNDO_BLOCK_INS:
        /* Delete range (e->row,e->col)-(e->row2,e->col2) */
        {
            int r0 = e->row, c0 = e->col;
            int r1 = e->row2, c1 = e->col2;
            if (r0 == r1) {
                char *s = app->content.lines[r0];
                memmove(s + c0, s + c1, strlen(s + c1) + 1);
            } else {
                /* keep prefix of r0 + suffix of r1 */
                const char *pfx  = app->content.lines[r0];
                const char *sfx  = app->content.lines[r1] + c1;
                char *merged = malloc(c0 + strlen(sfx) + 1);
                if (merged) {
                    memcpy(merged, pfx, c0);
                    strcpy(merged + c0, sfx);
                    line_set(app, r0, merged); free(merged);
                }
                for (int ri = r1; ri > r0; ri--)
                    content_delete_line(app, ri);
            }
        }
        app->cursor_row = e->row;
        app->cursor_col = e->col;
        break;

    case UNDO_LINE_DUP:
        /* Original was at row; duplicate was inserted at row+1. Remove row+1. */
        if (e->row + 1 < app->content.line_count)
            content_delete_line(app, e->row + 1);
        app->cursor_row = e->row;
        app->cursor_col = 0;
        break;

    case UNDO_LINE_DEL:
        /* Re-insert deleted text at row. If only one (empty) line remains,
           replace it with text instead of inserting. */
        if (e->text) {
            if (app->content.line_count == 1 &&
                app->content.lines[0][0] == '\0' && e->row == 0) {
                line_set(app, 0, e->text);
            } else {
                content_insert_line(app, e->row, e->text);
            }
            free(e->text); e->text = NULL;
        }
        app->cursor_row = e->row;
        app->cursor_col = 0;
        break;

    case UNDO_LINE_SWAP:
        /* Swap rows back */
        if (e->row  >= 0 && e->row  < app->content.line_count &&
            e->row2 >= 0 && e->row2 < app->content.line_count) {
            char *tmp = app->content.lines[e->row];
            app->content.lines[e->row]  = app->content.lines[e->row2];
            app->content.lines[e->row2] = tmp;
        }
        app->cursor_row = e->row;
        app->cursor_col = 0;
        break;
    }

    app->content_dirty = 1;
    cursor_clamp(app);
    cursor_scroll(app);
    sel_clear(app);
}

/* ---------------------------------------------------------------
   Editor: atomic edit operations
--------------------------------------------------------------- */
static void edit_insert_char(App *app, char ch) {
    int row = app->cursor_row, col = app->cursor_col;
    UndoEntry *e = undo_alloc(app);
    e->kind = UNDO_CHAR_INS;
    e->row  = row; e->col = col; e->col2 = 1;
    line_insert_char(app, row, col, ch);
    app->cursor_col++;
    app->content_dirty = 1;
}

static void edit_insert_utf8(App *app, const char *bytes, int nb) {
    int row = app->cursor_row, col = app->cursor_col;
    UndoEntry *e = undo_alloc(app);
    e->kind = UNDO_CHAR_INS;
    e->row = row; e->col = col; e->col2 = nb;
    line_insert_bytes(app, row, col, bytes, nb);
    app->cursor_col += nb;
    app->content_dirty = 1;
}

static void edit_join_lines(App *app, int row) {
    /* join lines[row] with lines[row+1] */
    if (row + 1 >= app->content.line_count) return;
    const char *a = app->content.lines[row];
    const char *b = app->content.lines[row + 1];
    int la = (int)strlen(a), lb = (int)strlen(b);
    char *joined = malloc(la + lb + 1);
    if (!joined) return;
    memcpy(joined, a, la);
    memcpy(joined + la, b, lb + 1);
    line_set(app, row, joined); free(joined);
    content_delete_line(app, row + 1);
}

static void edit_delete_sel(App *app);  /* forward decl */

static void edit_delete_before(App *app) {
    if (app->sel_active) { edit_delete_sel(app); return; }
    int row = app->cursor_row, col = app->cursor_col;
    if (col > 0) {
        const char *s = app->content.lines[row];
        int start = col - 1;
        while (start > 0 && ((unsigned char)s[start] & 0xC0) == 0x80) start--;
        int nb = col - start;
        UndoEntry *e = undo_alloc(app);
        e->kind = UNDO_CHAR_DEL;
        e->row = row; e->col = start;
        e->text = malloc(nb + 1);
        if (e->text) { memcpy(e->text, s + start, nb); e->text[nb] = '\0'; }
        for (int i = 0; i < nb; i++) line_delete_char(app, row, start);
        app->cursor_col = start;
    } else if (row > 0) {
        int prev_len = (int)strlen(app->content.lines[row - 1]);
        UndoEntry *e = undo_alloc(app);
        e->kind = UNDO_JOIN;
        e->row  = row - 1; e->col = prev_len;
        edit_join_lines(app, row - 1);
        app->cursor_row = row - 1;
        app->cursor_col = prev_len;
        cursor_scroll(app);
    }
    app->content_dirty = 1;
}

static void edit_delete_after(App *app) {
    if (app->sel_active) { edit_delete_sel(app); return; }
    int row = app->cursor_row, col = app->cursor_col;
    int linelen = (int)strlen(app->content.lines[row]);
    if (col < linelen) {
        const char *s = app->content.lines[row];
        int nb = utf8_char_bytes((unsigned char)s[col]);
        if (col + nb > linelen) nb = linelen - col;
        UndoEntry *e = undo_alloc(app);
        e->kind = UNDO_CHAR_DEL;
        e->row = row; e->col = col;
        e->text = malloc(nb + 1);
        if (e->text) { memcpy(e->text, s + col, nb); e->text[nb] = '\0'; }
        for (int i = 0; i < nb; i++) line_delete_char(app, row, col);
    } else if (row + 1 < app->content.line_count) {
        int cur_len = linelen;
        UndoEntry *e = undo_alloc(app);
        e->kind = UNDO_JOIN;
        e->row  = row; e->col = cur_len;
        edit_join_lines(app, row);
    }
    app->content_dirty = 1;
}

static void edit_split_line(App *app) {
    int row = app->cursor_row, col = app->cursor_col;
    UndoEntry *e = undo_alloc(app);
    e->kind = UNDO_SPLIT; e->row = row; e->col = col;

    const char *old = app->content.lines[row];
    char *tail = strdup(old + col);
    char *head = malloc(col + 1);
    if (!tail || !head) { free(tail); free(head); return; }
    memcpy(head, old, col); head[col] = '\0';
    line_set(app, row, head); free(head);
    content_insert_line(app, row + 1, tail); free(tail);
    app->cursor_row = row + 1;
    app->cursor_col = 0;
    cursor_scroll(app);
    app->content_dirty = 1;
}

/* ---------------------------------------------------------------
   Editor: selection operations
--------------------------------------------------------------- */
static void edit_copy_sel(App *app) {
    if (!app->sel_active) return;
    int r0,c0,r1,c1;
    sel_normalize(app,&r0,&c0,&r1,&c1);

    clipboard_free(app);

    int nlines = r1 - r0 + 1;
    app->clipboard = malloc(nlines * sizeof(char *));
    if (!app->clipboard) return;
    app->clipboard_count = nlines;

    for (int i = 0; i < nlines; i++) {
        int ri = r0 + i;
        const char *s = app->content.lines[ri];
        int slen = (int)strlen(s);
        int from = (i == 0)          ? c0   : 0;
        int to   = (i == nlines - 1) ? c1   : slen;
        int sz   = to - from;
        if (sz < 0) sz = 0;
        app->clipboard[i] = malloc(sz + 1);
        if (app->clipboard[i]) {
            memcpy(app->clipboard[i], s + from, sz);
            app->clipboard[i][sz] = '\0';
        }
    }
}

static void edit_delete_sel(App *app) {
    if (!app->sel_active) return;
    int r0,c0,r1,c1;
    sel_normalize(app,&r0,&c0,&r1,&c1);

    /* Build undo text: join selected lines with \n */
    int total = 0;
    int nlines = r1 - r0 + 1;
    for (int i = 0; i < nlines; i++) {
        int ri = r0 + i;
        const char *s = app->content.lines[ri];
        int from = (i == 0)          ? c0             : 0;
        int to   = (i == nlines - 1) ? c1             : (int)strlen(s);
        total += (to - from) + 1; /* +1 for \n or \0 */
    }
    char *undo_text = malloc(total + 1);
    if (undo_text) {
        int pos = 0;
        for (int i = 0; i < nlines; i++) {
            int ri = r0 + i;
            const char *s = app->content.lines[ri];
            int from = (i == 0)          ? c0             : 0;
            int to   = (i == nlines - 1) ? c1             : (int)strlen(s);
            int sz   = to - from; if (sz < 0) sz = 0;
            memcpy(undo_text + pos, s + from, sz);
            pos += sz;
            if (i < nlines - 1) undo_text[pos++] = '\n';
        }
        undo_text[pos] = '\0';
    }

    UndoEntry *e = undo_alloc(app);
    e->kind = UNDO_BLOCK_DEL;
    e->row  = r0; e->col  = c0;
    e->row2 = r1; e->col2 = c1;
    e->text = undo_text;

    /* Perform deletion */
    if (r0 == r1) {
        char *s = app->content.lines[r0];
        memmove(s + c0, s + c1, strlen(s + c1) + 1);
    } else {
        const char *pfx  = app->content.lines[r0];
        const char *sfx  = app->content.lines[r1] + c1;
        char *merged = malloc(c0 + strlen(sfx) + 1);
        if (merged) {
            memcpy(merged, pfx, c0);
            strcpy(merged + c0, sfx);
            line_set(app, r0, merged); free(merged);
        }
        for (int ri = r1; ri > r0; ri--)
            content_delete_line(app, ri);
    }

    app->cursor_row = r0;
    app->cursor_col = c0;
    sel_clear(app);
    cursor_scroll(app);
    app->content_dirty = 1;
}

static void edit_cut_sel(App *app) {
    edit_copy_sel(app);
    edit_delete_sel(app);
}

static void edit_paste(App *app) {
    if (!app->clipboard || app->clipboard_count == 0) return;

    int pr = app->cursor_row, pc = app->cursor_col;

    UndoEntry *e = undo_alloc(app);
    e->kind = UNDO_BLOCK_INS;
    e->row  = pr; e->col = pc;

    if (app->clipboard_count == 1) {
        /* Single-line paste: insert inline */
        int slen = (int)strlen(app->clipboard[0]);
        for (int i = 0; i < slen; i++)
            line_insert_char(app, pr, pc + i, app->clipboard[0][i]);
        app->cursor_col = pc + slen;
        e->row2 = pr; e->col2 = pc + slen;
    } else {
        /* Multi-line paste */
        const char *old = app->content.lines[pr];
        char *tail = strdup(old + pc);
        char *head = malloc(pc + strlen(app->clipboard[0]) + 1);
        if (!tail || !head) { free(tail); free(head); return; }
        memcpy(head, old, pc);
        strcpy(head + pc, app->clipboard[0]);
        line_set(app, pr, head); free(head);

        for (int i = 1; i < app->clipboard_count - 1; i++)
            content_insert_line(app, pr + i, app->clipboard[i]);

        int lr = pr + app->clipboard_count - 1;
        int last_len = (int)strlen(app->clipboard[app->clipboard_count - 1]);
        char *last = malloc(last_len + strlen(tail) + 1);
        if (last) {
            strcpy(last, app->clipboard[app->clipboard_count - 1]);
            strcat(last, tail);
            content_insert_line(app, lr, last); free(last);
        }
        free(tail);
        app->cursor_row = lr;
        app->cursor_col = last_len;
        e->row2 = lr; e->col2 = last_len;
        cursor_scroll(app);
    }
    app->content_dirty = 1;
}

static void edit_select_all(App *app) {
    app->sel_anchor_row = 0;
    app->sel_anchor_col = 0;
    app->sel_active     = 1;
    app->cursor_row     = app->content.line_count - 1;
    if (app->cursor_row < 0) app->cursor_row = 0;
    app->cursor_col     = (int)strlen(app->content.lines[app->cursor_row]);
    cursor_scroll(app);
}

/* ---------------------------------------------------------------
   Editor: Sublime-style line operations
--------------------------------------------------------------- */

/* Ctrl+L: select the current line (including its trailing newline) */
static void edit_select_line(App *app) {
    if (app->content.line_count == 0) return;
    app->sel_anchor_row = app->cursor_row;
    app->sel_anchor_col = 0;
    app->sel_active     = 1;
    if (app->cursor_row + 1 < app->content.line_count) {
        app->cursor_row++;
        app->cursor_col = 0;
    } else {
        app->cursor_col = (int)strlen(app->content.lines[app->cursor_row]);
    }
    cursor_scroll(app);
}

/* Ctrl+D: duplicate the current line below itself */
static void edit_duplicate_line(App *app) {
    int row = app->cursor_row;
    if (row < 0 || row >= app->content.line_count) return;
    const char *src = app->content.lines[row];

    UndoEntry *e = undo_alloc(app);
    e->kind = UNDO_LINE_DUP;
    e->row  = row;

    content_insert_line(app, row + 1, src);
    app->cursor_row = row + 1;
    cursor_scroll(app);
    sel_clear(app);
    app->content_dirty = 1;
}

/* Ctrl+K: delete the current line */
static void edit_delete_line(App *app) {
    int row = app->cursor_row;
    if (row < 0 || row >= app->content.line_count) return;

    UndoEntry *e = undo_alloc(app);
    e->kind = UNDO_LINE_DEL;
    e->row  = row;
    e->text = strdup(app->content.lines[row]);

    if (app->content.line_count == 1) {
        line_set(app, 0, "");
    } else {
        content_delete_line(app, row);
        if (app->cursor_row >= app->content.line_count)
            app->cursor_row = app->content.line_count - 1;
    }
    app->cursor_col = 0;
    cursor_scroll(app);
    sel_clear(app);
    app->content_dirty = 1;
}

/* Alt+Up / Alt+Down: swap current line with neighbor */
static void edit_move_line_up(App *app) {
    int row = app->cursor_row;
    if (row <= 0 || row >= app->content.line_count) return;
    char *tmp = app->content.lines[row];
    app->content.lines[row]     = app->content.lines[row - 1];
    app->content.lines[row - 1] = tmp;

    UndoEntry *e = undo_alloc(app);
    e->kind = UNDO_LINE_SWAP;
    e->row  = row - 1; e->row2 = row;

    app->cursor_row--;
    cursor_scroll(app);
    sel_clear(app);
    app->content_dirty = 1;
}

static void edit_move_line_down(App *app) {
    int row = app->cursor_row;
    if (row < 0 || row + 1 >= app->content.line_count) return;
    char *tmp = app->content.lines[row];
    app->content.lines[row]     = app->content.lines[row + 1];
    app->content.lines[row + 1] = tmp;

    UndoEntry *e = undo_alloc(app);
    e->kind = UNDO_LINE_SWAP;
    e->row  = row; e->row2 = row + 1;

    app->cursor_row++;
    cursor_scroll(app);
    sel_clear(app);
    app->content_dirty = 1;
}

/* ---------------------------------------------------------------
   Editor: in-file find (MODE_EDIT_FIND)
--------------------------------------------------------------- */

/* Case-insensitive substring search starting at `from_col` in `line`.
   Returns match column, or -1 if no match. */
static int find_in_line_from(const char *line, const char *query, int from_col) {
    int qlen = (int)strlen(query);
    if (qlen == 0) return -1;
    int llen = (int)strlen(line);
    for (int i = from_col; i + qlen <= llen; i++) {
        int ok = 1;
        for (int j = 0; j < qlen; j++) {
            if (tolower((unsigned char)line[i + j]) !=
                tolower((unsigned char)query[j])) { ok = 0; break; }
        }
        if (ok) return i;
    }
    return -1;
}

/* Same but searches backward from from_col (inclusive end). */
static int find_in_line_back(const char *line, const char *query, int upto_col) {
    int qlen = (int)strlen(query);
    if (qlen == 0) return -1;
    int last = upto_col - qlen;
    int llen = (int)strlen(line);
    if (last > llen - qlen) last = llen - qlen;
    for (int i = last; i >= 0; i--) {
        int ok = 1;
        for (int j = 0; j < qlen; j++) {
            if (tolower((unsigned char)line[i + j]) !=
                tolower((unsigned char)query[j])) { ok = 0; break; }
        }
        if (ok) return i;
    }
    return -1;
}

/* Count total matches in the whole file. */
static int find_total_matches(const App *app) {
    if (app->find_query_len == 0) return 0;
    int total = 0;
    for (int r = 0; r < app->content.line_count; r++) {
        const char *line = app->content.lines[r];
        int from = 0, m;
        while ((m = find_in_line_from(line, app->find_query, from)) >= 0) {
            total++;
            from = m + app->find_query_len;
        }
    }
    return total;
}

/* Jump cursor to next match starting AFTER current position; wraps to top. */
static void find_jump_next(App *app) {
    if (app->find_query_len == 0) return;
    int start_row = app->cursor_row;
    int start_col = app->cursor_col + 1;
    for (int wrap = 0; wrap < 2; wrap++) {
        for (int r = start_row; r < app->content.line_count; r++) {
            int c = (r == start_row) ? start_col : 0;
            int m = find_in_line_from(app->content.lines[r], app->find_query, c);
            if (m >= 0) {
                app->cursor_row = r;
                app->cursor_col = m;
                cursor_scroll(app);
                return;
            }
        }
        start_row = 0; start_col = 0;
    }
}

/* Jump cursor to previous match BEFORE current position; wraps to bottom. */
static void find_jump_prev(App *app) {
    if (app->find_query_len == 0) return;
    int start_row = app->cursor_row;
    int start_col = app->cursor_col;
    for (int wrap = 0; wrap < 2; wrap++) {
        for (int r = start_row; r >= 0; r--) {
            const char *line = app->content.lines[r];
            int upto = (r == start_row) ? start_col
                                         : (int)strlen(line);
            int m = find_in_line_back(line, app->find_query, upto);
            if (m >= 0) {
                app->cursor_row = r;
                app->cursor_col = m;
                cursor_scroll(app);
                return;
            }
        }
        start_row = app->content.line_count - 1;
        start_col = INT_MAX / 2;
    }
}

/* As query changes, jump to first match from current cursor. */
static void find_jump_first_from_cursor(App *app) {
    if (app->find_query_len == 0) return;
    int start_row = app->cursor_row;
    int start_col = app->cursor_col;
    for (int wrap = 0; wrap < 2; wrap++) {
        for (int r = start_row; r < app->content.line_count; r++) {
            int c = (r == start_row) ? start_col : 0;
            int m = find_in_line_from(app->content.lines[r], app->find_query, c);
            if (m >= 0) {
                app->cursor_row = r;
                app->cursor_col = m;
                cursor_scroll(app);
                return;
            }
        }
        start_row = 0; start_col = 0;
    }
}

/* ---------------------------------------------------------------
   Editor: save file
--------------------------------------------------------------- */
static int save_file(App *app) {
    if (app->open_file < 0) return 0;
    const char *path = app->files[app->open_file].path;
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    for (int i = 0; i < app->content.line_count; i++)
        fprintf(f, "%s\n", app->content.lines[i]);
    fclose(f);
    app->content_dirty = 0;
    app->files[app->open_file].line_count = app->content.line_count;
    return 1;
}

/* ---------------------------------------------------------------
   Background scan thread
--------------------------------------------------------------- */

/* Called by scan_dir every SCAN_BATCH_SIZE files; runs in scan thread */
static void scan_progress_cb(const FileMeta *files, int count, void *ctx) {
    App *app = ctx;
    app->scan_live_count = count;   /* always update (no mutex: volatile) */

    if (count < app->scan_batch_threshold) return;
    app->scan_batch_threshold = count * 2;  /* next snapshot when file count doubles */

    FileMeta *snap = malloc(count * sizeof(FileMeta));
    if (!snap) return;
    memcpy(snap, files, count * sizeof(FileMeta));

    pthread_mutex_lock(&app->scan_mutex);
    free(app->scan_pending);
    app->scan_pending       = snap;
    app->scan_pending_count = count;
    app->scan_batch_ready   = 1;
    pthread_mutex_unlock(&app->scan_mutex);
}

static void *scan_thread_fn(void *arg) {
    ScanArgs *sa = arg;
    App *app = sa->app;

    int count = 0;
    char (*disc_arr)[META_SUBDIR_LEN] = NULL;
    int disc_count = 0;

    /* For merge mode, don't use progress callback (subdirs are small).
       For replace mode, use the progress callback for progressive display. */
    ScanProgressFn pfn = (sa->merge_mode == 0) ? scan_progress_cb : NULL;

    FileMeta *result = scan_dir(sa->dir, &count,
                                (const char * const *)sa->exts, sa->ext_count,
                                sa->max_depth,
                                &disc_arr, &disc_count,
                                pfn, app,
                                &app->scan_cancel);

    if (sa->merge_mode == 1) {
        /* Fix up subdir paths: prepend subdir_prefix */
        for (int i = 0; i < count; i++) {
            char new_sub[META_SUBDIR_LEN];
            if (result[i].subdir[0])
                snprintf(new_sub, META_SUBDIR_LEN, "%s/%s",
                         sa->subdir_prefix, result[i].subdir);
            else
                strncpy(new_sub, sa->subdir_prefix, META_SUBDIR_LEN - 1);
            new_sub[META_SUBDIR_LEN - 1] = '\0';
            strncpy(result[i].subdir, new_sub, META_SUBDIR_LEN - 1);
        }
        /* Fix up discovered boundary dirs similarly */
        for (int i = 0; i < disc_count; i++) {
            char new_sub[META_SUBDIR_LEN];
            if (disc_arr[i][0])
                snprintf(new_sub, META_SUBDIR_LEN, "%s/%s",
                         sa->subdir_prefix, disc_arr[i]);
            else
                strncpy(new_sub, sa->subdir_prefix, META_SUBDIR_LEN - 1);
            new_sub[META_SUBDIR_LEN - 1] = '\0';
            strncpy(disc_arr[i], new_sub, META_SUBDIR_LEN - 1);
        }
    }

    /* Store sorted final result; clear any unread partial batch */
    pthread_mutex_lock(&app->scan_mutex);
    free(app->scan_pending);
    app->scan_pending       = result;
    app->scan_pending_count = count;
    free(app->scan_pending_disc);
    app->scan_pending_disc       = disc_arr;
    app->scan_pending_disc_count = disc_count;
    app->scan_batch_ready   = 0;
    app->scan_done          = 1;
    pthread_mutex_unlock(&app->scan_mutex);

    free(sa);
    return NULL;
}

/* ---- merge sort comparator (mirrors meta.c cmp_subdir_filename) ---- */
static int cmp_filemeta_subdir_name(const void *a, const void *b) {
    const FileMeta *fa = (const FileMeta *)a;
    const FileMeta *fb = (const FileMeta *)b;
    int c = strcmp(fa->subdir, fb->subdir);
    return c ? c : strcmp(fa->filename, fb->filename);
}

/* ---- lazy-load helpers ---- */

static int subdir_is_loaded(App *app, const char *subdir) {
    for (int i = 0; i < app->loaded_dir_count; i++)
        if (strcmp(app->loaded_dirs[i], subdir) == 0) return 1;
    return 0;
}

static int is_boundary_dir(App *app, const char *subdir) {
    for (int i = 0; i < app->boundary_dir_count; i++)
        if (strcmp(app->boundary_dirs[i], subdir) == 0) return 1;
    return 0;
}

/* Add subdir to loaded_dirs (sorted, no duplicates) */
static void loaded_dirs_add(App *app, const char *subdir) {
    if (subdir_is_loaded(app, subdir)) return;
    if (app->loaded_dir_count >= app->loaded_dir_cap) {
        int nc = app->loaded_dir_cap ? app->loaded_dir_cap * 2 : 16;
        char (*tmp)[META_SUBDIR_LEN] = realloc(app->loaded_dirs, nc * sizeof(*tmp));
        if (!tmp) return;
        app->loaded_dirs    = tmp;
        app->loaded_dir_cap = nc;
    }
    strncpy(app->loaded_dirs[app->loaded_dir_count++], subdir, META_SUBDIR_LEN - 1);
}

static void boundary_dirs_free(App *app) {
    free(app->boundary_dirs);
    app->boundary_dirs      = NULL;
    app->boundary_dir_count = 0;
}

static void loaded_dirs_free(App *app) {
    free(app->loaded_dirs);
    app->loaded_dirs      = NULL;
    app->loaded_dir_count = 0;
    app->loaded_dir_cap   = 0;
}

/* Forward declaration */
static void start_subdir_scan(App *app, const char *subdir);

/* Apply completed scan results (call after joining the thread) */
static void apply_scan_result(App *app) {
    pthread_mutex_lock(&app->scan_mutex);
    FileMeta *new_files       = app->scan_pending;
    int       new_count       = app->scan_pending_count;
    char    (*disc_arr)[META_SUBDIR_LEN] = app->scan_pending_disc;
    int       disc_count      = app->scan_pending_disc_count;
    int       merge_mode      = 0; /* determined by loading_subdir */
    app->scan_pending              = NULL;
    app->scan_pending_count        = 0;
    app->scan_pending_disc         = NULL;
    app->scan_pending_disc_count   = 0;
    app->scan_done                 = 0;
    app->scan_batch_ready          = 0;
    pthread_mutex_unlock(&app->scan_mutex);

    /* If loading_subdir is set, this was a merge-mode scan */
    merge_mode = (app->loading_subdir[0] != '\0');

    if (!merge_mode) {
        /* Replace mode: swap in new file list */
        free(app->files);
        app->files      = new_files;
        app->file_count = new_count;

        /* Update boundary dirs */
        boundary_dirs_free(app);
        app->boundary_dirs      = disc_arr;
        app->boundary_dir_count = disc_count;

        /* Reset loaded_dirs: root ("") was loaded */
        loaded_dirs_free(app);
        loaded_dirs_add(app, "");

        app->loading_subdir[0] = '\0';
    } else {
        /* Merge mode: insert new_files into existing app->files[] */
        char loaded_sub[META_SUBDIR_LEN];
        strncpy(loaded_sub, app->loading_subdir, META_SUBDIR_LEN - 1);
        loaded_sub[META_SUBDIR_LEN - 1] = '\0';
        app->loading_subdir[0] = '\0';

        if (new_files && new_count > 0) {
            int total = app->file_count + new_count;
            FileMeta *merged = realloc(app->files, total * sizeof(FileMeta));
            if (merged) {
                /* Deduplicate: skip incoming entries whose path already exists */
                int added = 0;
                for (int i = 0; i < new_count; i++) {
                    int dup = 0;
                    for (int j = 0; j < app->file_count; j++) {
                        if (strcmp(new_files[i].path, merged[j].path) == 0) {
                            dup = 1; break;
                        }
                    }
                    if (!dup) {
                        merged[app->file_count + added] = new_files[i];
                        added++;
                    }
                }
                app->files      = merged;
                app->file_count = app->file_count + added;
                qsort(app->files, app->file_count, sizeof(FileMeta),
                      cmp_filemeta_subdir_name);
            }
        }
        free(new_files);

        /* Update boundary_dirs: remove loaded_sub, add new disc dirs */
        /* Remove loaded_sub from boundary_dirs */
        for (int i = 0; i < app->boundary_dir_count; i++) {
            if (strcmp(app->boundary_dirs[i], loaded_sub) == 0) {
                /* Shift remaining entries left */
                memmove(&app->boundary_dirs[i], &app->boundary_dirs[i + 1],
                        (app->boundary_dir_count - i - 1) * sizeof(*app->boundary_dirs));
                app->boundary_dir_count--;
                break;
            }
        }
        /* Add new disc dirs (grow array if needed) */
        if (disc_arr && disc_count > 0) {
            int new_total = app->boundary_dir_count + disc_count;
            char (*tmp)[META_SUBDIR_LEN] = realloc(app->boundary_dirs,
                                                     new_total * sizeof(*tmp));
            if (tmp) {
                app->boundary_dirs = tmp;
                for (int i = 0; i < disc_count; i++) {
                    /* Only add if not already present */
                    int dup = 0;
                    for (int j = 0; j < app->boundary_dir_count; j++) {
                        if (strcmp(app->boundary_dirs[j], disc_arr[i]) == 0) {
                            dup = 1; break;
                        }
                    }
                    if (!dup) {
                        strncpy(app->boundary_dirs[app->boundary_dir_count++],
                                disc_arr[i], META_SUBDIR_LEN - 1);
                    }
                }
            }
        }
        free(disc_arr);

        /* Mark the loaded subdir */
        loaded_dirs_add(app, loaded_sub);
    }

    free(app->filtered);
    app->filtered = app->file_count > 0
                    ? malloc(app->file_count * sizeof(int)) : NULL;
    app->filtered_count = 0;
}

/* Check for partial batches or final completion; apply whichever is ready */
static void check_scan_done(App *app) {
    if (!app->scan_running) return;

    pthread_mutex_lock(&app->scan_mutex);
    int   batch_ready = app->scan_batch_ready;
    int   is_done     = app->scan_done;
    FileMeta *snap    = NULL;
    int   snap_count  = 0;

    if (batch_ready) {
        snap                    = app->scan_pending;
        snap_count              = app->scan_pending_count;
        app->scan_pending       = NULL;
        app->scan_pending_count = 0;
        app->scan_batch_ready   = 0;
    }
    pthread_mutex_unlock(&app->scan_mutex);

    /* Apply partial batch (progressive display) */
    if (snap) {
        free(app->files);
        app->files          = snap;
        app->file_count     = snap_count;
        free(app->filtered);
        app->filtered       = snap_count > 0 ? malloc(snap_count * sizeof(int)) : NULL;
        app->filtered_count = 0;
        update_search(app);
        /* No build_tree here — too expensive for large partial results */
    }

    /* Apply final sorted result and join thread */
    if (is_done) {
        pthread_join(app->scan_thread, NULL);
        app->scan_running = 0;
        apply_scan_result(app);
        update_search(app);
        build_tree(app);
        /* Process next queued subdir expansion, if any */
        if (app->expand_queue_count > 0) {
            char next[META_SUBDIR_LEN];
            strncpy(next, app->expand_queue[0], META_SUBDIR_LEN - 1);
            next[META_SUBDIR_LEN - 1] = '\0';
            memmove(&app->expand_queue[0], &app->expand_queue[1],
                    (app->expand_queue_count - 1) * sizeof(app->expand_queue[0]));
            app->expand_queue_count--;
            start_subdir_scan(app, next);
        }
    }
}

/* Launch a background scan for the current directory + filter.
   max_depth: -1 = unlimited, 2 = shallow startup scan */
static void start_scan(App *app, int max_depth) {
    /* Cancel + join any existing thread before starting a new one */
    if (app->scan_running) {
        app->scan_cancel = 1;
        pthread_join(app->scan_thread, NULL);
        app->scan_running = 0;
        app->scan_cancel  = 0;
        pthread_mutex_lock(&app->scan_mutex);
        free(app->scan_pending);
        app->scan_pending       = NULL;
        app->scan_pending_count = 0;
        free(app->scan_pending_disc);
        app->scan_pending_disc       = NULL;
        app->scan_pending_disc_count = 0;
        app->scan_done          = 0;
        app->scan_batch_ready   = 0;
        pthread_mutex_unlock(&app->scan_mutex);
        /* If we interrupted a subdir scan, clear its state so apply_scan_result
           doesn't mistake the incoming full-scan result as a merge. */
        app->loading_subdir[0]  = '\0';
        app->expand_queue_count = 0;
    }

    const char *dir = (app->in_lib_view && app->lib_dir[0])
                      ? app->lib_dir : app->scan_dir_path;
    if (!dir[0]) return;

    ScanArgs *sa = malloc(sizeof(ScanArgs));
    if (!sa) return;
    memset(sa, 0, sizeof(*sa));

    sa->app = app;
    strncpy(sa->dir, dir, META_PATH_LEN - 1);
    sa->dir[META_PATH_LEN - 1] = '\0';

    /* Copy the extension filter so the thread has its own copy */
    strncpy(sa->exts_buf, app->filter_buf, sizeof(sa->exts_buf) - 1);
    sa->exts_buf[sizeof(sa->exts_buf) - 1] = '\0';
    sa->ext_count = 0;
    if (app->filter_ext_count > 0) {
        /* Re-parse pointers into our local exts_buf */
        char *save = NULL;
        char *tok = strtok_r(sa->exts_buf, ",", &save);
        while (tok && sa->ext_count < 16) {
            while (*tok == ' ' || *tok == '.') tok++;
            char *end = tok + strlen(tok) - 1;
            while (end > tok && (*end == ' ' || *end == '.')) *end-- = '\0';
            if (*tok) sa->exts[sa->ext_count++] = tok;
            tok = strtok_r(NULL, ",", &save);
        }
    }

    sa->max_depth  = max_depth;
    sa->merge_mode = 0;

    app->scan_done            = 0;
    app->scan_live_count      = 0;
    app->scan_batch_threshold = SCAN_BATCH_SIZE;
    app->scan_running = 1;
    if (pthread_create(&app->scan_thread, NULL, scan_thread_fn, sa) != 0) {
        app->scan_running = 0;
        free(sa);
    }
}

/* -----------------------------------------------------------------------
   start_subdir_scan — on-demand lazy expansion of a single boundary dir
----------------------------------------------------------------------- */
static void start_subdir_scan(App *app, const char *subdir) {
    /* Already loaded or already loading: nothing to do */
    if (subdir_is_loaded(app, subdir)) return;
    if (app->loading_subdir[0] &&
        strcmp(app->loading_subdir, subdir) == 0) return;

    /* If another scan is running, queue this one */
    if (app->scan_running) {
        if (app->expand_queue_count < 8) {
            /* Don't queue duplicates */
            int dup = 0;
            for (int i = 0; i < app->expand_queue_count; i++) {
                if (strcmp(app->expand_queue[i], subdir) == 0) { dup = 1; break; }
            }
            if (!dup)
                strncpy(app->expand_queue[app->expand_queue_count++],
                        subdir, META_SUBDIR_LEN - 1);
        }
        return;
    }

    strncpy(app->loading_subdir, subdir, META_SUBDIR_LEN - 1);
    app->loading_subdir[META_SUBDIR_LEN - 1] = '\0';

    ScanArgs *sa = calloc(1, sizeof(ScanArgs));
    if (!sa) { app->loading_subdir[0] = '\0'; return; }

    sa->app = app;

    /* Build absolute path to the subdir (avoid double slash when root is "/") */
    const char *root = (app->in_lib_view && app->lib_dir[0])
                       ? app->lib_dir : app->scan_dir_path;
    size_t rlen = strlen(root);
    if (rlen > 0 && root[rlen - 1] == '/')
        snprintf(sa->dir, META_PATH_LEN, "%s%s", root, subdir);
    else
        snprintf(sa->dir, META_PATH_LEN, "%s/%s", root, subdir);
    strncpy(sa->subdir_prefix, subdir, META_SUBDIR_LEN - 1);
    sa->max_depth  = 1;
    sa->merge_mode = 1;

    /* Copy extension filter */
    strncpy(sa->exts_buf, app->filter_buf, sizeof(sa->exts_buf) - 1);
    sa->exts_buf[sizeof(sa->exts_buf) - 1] = '\0';
    sa->ext_count = 0;
    if (app->filter_ext_count > 0) {
        char *save = NULL;
        char *tok = strtok_r(sa->exts_buf, ",", &save);
        while (tok && sa->ext_count < 16) {
            while (*tok == ' ' || *tok == '.') tok++;
            char *end = tok + strlen(tok) - 1;
            while (end > tok && (*end == ' ' || *end == '.')) *end-- = '\0';
            if (*tok) sa->exts[sa->ext_count++] = tok;
            tok = strtok_r(NULL, ",", &save);
        }
    }

    app->scan_done            = 0;
    app->scan_live_count      = 0;
    app->scan_batch_threshold = SCAN_BATCH_SIZE;
    app->scan_running = 1;
    if (pthread_create(&app->scan_thread, NULL, scan_thread_fn, sa) != 0) {
        app->scan_running  = 0;
        app->loading_subdir[0] = '\0';
        free(sa);
    }
}

/* ---------------------------------------------------------------
   Rescan + filter helpers
--------------------------------------------------------------- */

static void rescan(App *app) {
    /* Clear current state immediately so UI shows empty / scanning */
    free(app->files);
    app->files      = NULL;
    app->file_count = 0;
    free(app->filtered);
    app->filtered       = NULL;
    app->filtered_count = 0;
    app->list_sel    = 0;
    app->list_offset = 0;
    /* Reset lazy-load state */
    boundary_dirs_free(app);
    loaded_dirs_free(app);
    app->expand_queue_count = 0;
    app->loading_subdir[0]  = '\0';
    app->open_file   = -1;
    content_free(&app->content);
    memset(app->query, 0, sizeof(app->query));
    app->query_len   = 0;
    build_tree(app);

    /* Library has few files — scan fully; CWD starts shallow */
    if (app->in_lib_view) {
        app->scan_full = 1;
        start_scan(app, -1);
    } else {
        app->scan_full = 0;
        start_scan(app, 2);
    }
}

static void apply_filter_and_rescan(App *app) {
    strncpy(app->filter_buf, app->filter_input, sizeof(app->filter_buf) - 1);
    app->filter_buf[sizeof(app->filter_buf) - 1] = '\0';
    app->filter_ext_count = 0;
    char *filter_save = NULL;
    char *tok = strtok_r(app->filter_buf, ",", &filter_save);
    while (tok && app->filter_ext_count < 16) {
        while (*tok == ' ' || *tok == '.') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && (*end == ' ' || *end == '.')) *end-- = '\0';
        if (*tok) app->filter_exts[app->filter_ext_count++] = tok;
        tok = strtok_r(NULL, ",", &filter_save);
    }
    rescan(app);
}

static void handle_goto(App *app, int key) {
    switch (key) {
    case '\n': case '\r': {
        struct stat st;
        if (stat(app->goto_buf, &st) == 0 && S_ISDIR(st.st_mode)) {
            strncpy(app->scan_dir_path, app->goto_buf, META_PATH_LEN - 1);
            app->in_lib_view = 0;
            app->mode        = MODE_LIST;
            curs_set(0);
            rescan(app);
        }
        /* if invalid dir, stay in MODE_GOTO so user can correct */
        break;
    }
    case 27: /* Esc */
        app->mode = MODE_LIST;
        curs_set(0);
        break;
    case KEY_BACKSPACE: case 127: case '\b':
        if (app->goto_len > 0)
            app->goto_buf[--app->goto_len] = '\0';
        break;
    default:
        if (key >= 32 && key < 127 &&
            app->goto_len < (int)sizeof(app->goto_buf) - 2) {
            app->goto_buf[app->goto_len++] = (char)key;
            app->goto_buf[app->goto_len]   = '\0';
        }
        break;
    }
}

static void handle_filter(App *app, int key) {
    switch (key) {
    case '\n': case '\r':
        apply_filter_and_rescan(app);
        app->mode = MODE_LIST;
        break;
    case 27:
        app->mode = MODE_LIST;
        break;
    case KEY_BACKSPACE: case 127: case '\b':
        if (app->filter_input_len > 0)
            app->filter_input[--app->filter_input_len] = '\0';
        break;
    default:
        if (key >= 32 && key < 127 &&
            app->filter_input_len < (int)sizeof(app->filter_input) - 2) {
            app->filter_input[app->filter_input_len++] = (char)key;
            app->filter_input[app->filter_input_len]   = '\0';
        }
        break;
    }
}

/* Build filter_input string from current filter state for display/editing */
static void init_filter_input(App *app) {
    app->filter_input[0]  = '\0';
    app->filter_input_len = 0;
    for (int i = 0; i < app->filter_ext_count; i++) {
        if (i > 0 && app->filter_input_len < (int)sizeof(app->filter_input) - 2)
            app->filter_input[app->filter_input_len++] = ',';
        strncat(app->filter_input, app->filter_exts[i],
                sizeof(app->filter_input) - app->filter_input_len - 1);
        app->filter_input_len = (int)strlen(app->filter_input);
    }
}

/* ---------------------------------------------------------------
   New-file and delete helpers
--------------------------------------------------------------- */

/* Insert one file into app->files[] (sorted by subdir/filename) and refresh
   the tree without a full rescan. Falls back to rescan() on alloc failure. */
static void insert_file_entry(App *app, const char *full_path,
                              const char *subdir, const char *filename)
{
    int new_count = app->file_count + 1;
    FileMeta *nf = realloc(app->files, new_count * sizeof(FileMeta));
    if (!nf) { rescan(app); return; }
    app->files = nf;

    int *nfilt = realloc(app->filtered, new_count * sizeof(int));
    if (!nfilt) { rescan(app); return; }
    app->filtered = nfilt;

    FileMeta fm;
    memset(&fm, 0, sizeof(fm));
    strncpy(fm.path,     full_path, META_PATH_LEN - 1);
    strncpy(fm.filename, filename,  META_NAME_LEN - 1);
    strncpy(fm.subdir,   subdir,    META_SUBDIR_LEN - 1);
    strncpy(fm.title,    filename,  META_TITLE_LEN - 1);
    char *dot = strrchr(fm.title, '.');
    if (dot) *dot = '\0';

    int pos = 0;
    while (pos < app->file_count) {
        int c = strcmp(app->files[pos].subdir, subdir);
        if (c > 0) break;
        if (c == 0 && strcmp(app->files[pos].filename, filename) >= 0) break;
        pos++;
    }

    if (pos < app->file_count)
        memmove(&app->files[pos + 1], &app->files[pos],
                (app->file_count - pos) * sizeof(FileMeta));
    app->files[pos] = fm;
    app->file_count = new_count;

    if (app->open_file >= pos) app->open_file++;

    update_search(app);
    build_tree(app);

    for (int i = 0; i < app->tree_view_count; i++) {
        int vi = app->tree_view[i];
        if (app->tree_all[vi].kind == NODE_FILE &&
            app->tree_all[vi].file_idx == pos) {
            app->tree_sel = i;
            int view_h = PANEL_H - META_PANEL_H - 1;
            if (app->tree_sel < app->tree_offset)
                app->tree_offset = app->tree_sel;
            if (app->tree_sel >= app->tree_offset + view_h)
                app->tree_offset = app->tree_sel - view_h + 1;
            break;
        }
    }
}

/* Remove one file from app->files[] by index and refresh the tree. */
static void remove_file_entry(App *app, int file_idx)
{
    if (file_idx < 0 || file_idx >= app->file_count) return;

    if (file_idx < app->file_count - 1)
        memmove(&app->files[file_idx], &app->files[file_idx + 1],
                (app->file_count - file_idx - 1) * sizeof(FileMeta));
    app->file_count--;

    if (app->open_file > file_idx)
        app->open_file--;

    update_search(app);
    build_tree(app);
}

/* Return the subdir (relative to scan_dir_path) to create a new file in.
   In tree mode: uses the selected node's directory.
   In filtered/search mode: root of scan_dir_path. */
static const char *new_file_target_subdir(const App *app) {
    if (app->query[0] != '\0') return "";
    if (app->tree_view_count == 0) return "";
    int vi = app->tree_view[app->tree_sel];
    const TreeNode *n = &app->tree_all[vi];
    if (n->kind == NODE_DIR)  return n->subdir;
    if (n->file_idx >= 0)     return app->files[n->file_idx].subdir;
    return "";
}

static void enter_new_file_mode(App *app) {
    app->new_file_buf[0] = '\0';
    app->new_file_len    = 0;
    app->new_file_msg[0] = '\0';
    app->mode = MODE_NEW_FILE;
    curs_set(1);
}

static void handle_new_file(App *app, int key) {
    switch (key) {
    case 27: /* Esc — cancel */
        app->mode = MODE_LIST;
        curs_set(0);
        break;
    case '\n': case '\r': {
        if (app->new_file_len == 0) { app->mode = MODE_LIST; curs_set(0); break; }
        /* Reject path traversal: no slashes, no ".." component */
        if (strchr(app->new_file_buf, '/') ||
            strcmp(app->new_file_buf, "..") == 0 ||
            strncmp(app->new_file_buf, "../", 3) == 0) {
            snprintf(app->new_file_msg, sizeof(app->new_file_msg),
                     "Invalid filename: no path separators allowed");
            break;
        }
        const char *sub = new_file_target_subdir(app);
        char full[META_PATH_LEN];
        if (sub[0])
            snprintf(full, sizeof(full), "%s/%s/%s",
                     app->scan_dir_path, sub, app->new_file_buf);
        else
            snprintf(full, sizeof(full), "%s/%s",
                     app->scan_dir_path, app->new_file_buf);
        /* Refuse to overwrite an existing file */
        struct stat st;
        if (stat(full, &st) == 0) {
            snprintf(app->new_file_msg, sizeof(app->new_file_msg),
                     "Already exists: %s", app->new_file_buf);
            break;
        }
        FILE *f = fopen(full, "w");
        if (!f) {
            snprintf(app->new_file_msg, sizeof(app->new_file_msg),
                     "Error: %s", strerror(errno));
            break;
        }
        fclose(f);
        curs_set(0);
        insert_file_entry(app, full, sub, app->new_file_buf);
        app->mode = MODE_LIST;
        break;
    }
    case KEY_BACKSPACE: case 127: case '\b':
        if (app->new_file_len > 0)
            app->new_file_buf[--app->new_file_len] = '\0';
        app->new_file_msg[0] = '\0';
        break;
    default:
        if (key >= 32 && key < 127 && key != '/' &&
            app->new_file_len < (int)sizeof(app->new_file_buf) - 2) {
            app->new_file_buf[app->new_file_len++] = (char)key;
            app->new_file_buf[app->new_file_len]   = '\0';
            app->new_file_msg[0] = '\0';
        }
        break;
    }
}

static void enter_confirm_del(App *app, int file_idx, int prev_mode) {
    app->del_pending_idx = file_idx;
    if (file_idx >= 0 && file_idx < app->file_count)
        strncpy(app->del_pending_path, app->files[file_idx].path, META_PATH_LEN - 1);
    else
        app->del_pending_path[0] = '\0';
    app->del_prev_mode = prev_mode;
    app->mode          = MODE_CONFIRM_DEL;
}

static void handle_confirm_del(App *app, int key) {
    if (key == 'y' || key == 'Y') {
        if (app->del_pending_path[0]) {
            int del_idx = app->del_pending_idx;
            unlink(app->del_pending_path);
            if (app->open_file >= 0 &&
                strcmp(app->files[app->open_file].path, app->del_pending_path) == 0) {
                app->open_file = -1;
                content_free(&app->content);
                bat_free(app);
            }
            remove_file_entry(app, del_idx);
        }
        app->del_pending_idx  = -1;
        app->del_pending_path[0] = '\0';
        app->mode = MODE_LIST;
    } else {
        app->del_pending_idx  = -1;
        app->del_pending_path[0] = '\0';
        app->mode = app->del_prev_mode;
    }
}

/* ---------------------------------------------------------------
   Build helpers
--------------------------------------------------------------- */

/* Header → linker flag(s) mapping.
   Each entry maps an #include <hdr> to one or more -l flags.
   Entries with multiple flags are space-separated. */
static const struct { const char *hdr; const char *flags; } LIB_MAP[] = {
    /* Math */
    {"math.h",                    "-lm"},
    {"complex.h",                 "-lm"},
    {"tgmath.h",                  "-lm"},
    /* Threads / sync */
    {"pthread.h",                 "-lpthread"},
    {"semaphore.h",               "-lpthread"},
    /* Terminal / curses */
    {"ncurses.h",                 "-lncurses"},
    {"ncurses/ncurses.h",         "-lncurses"},
    {"curses.h",                  "-lncurses"},
    {"term.h",                    "-lncurses"},
    {"panel.h",                   "-lncurses -lpanel"},
    {"form.h",                    "-lncurses -lform"},
    {"menu.h",                    "-lncurses -lmenu"},
    /* PTY — Linux uses pty.h, macOS uses util.h */
    {"pty.h",                     "-lutil"},
    {"util.h",                    "-lutil"},
    {"libutil.h",                 "-lutil"},
    /* Dynamic loading */
    {"dlfcn.h",                   "-ldl"},
    /* POSIX real-time (clock_gettime, mq_open, aio on older Linux) */
    {"mqueue.h",                  "-lrt"},
    {"aio.h",                     "-lrt"},
    /* Crypto / TLS */
    {"openssl/ssl.h",             "-lssl -lcrypto"},
    {"openssl/crypto.h",          "-lcrypto"},
    {"openssl/md5.h",             "-lcrypto"},
    {"openssl/sha.h",             "-lcrypto"},
    {"openssl/sha256.h",          "-lcrypto"},
    {"openssl/evp.h",             "-lssl -lcrypto"},
    {"openssl/rsa.h",             "-lcrypto"},
    {"openssl/aes.h",             "-lcrypto"},
    {"openssl/err.h",             "-lssl -lcrypto"},
    {"openssl/rand.h",            "-lcrypto"},
    {"openssl/hmac.h",            "-lcrypto"},
    {"openssl/x509.h",            "-lssl -lcrypto"},
    {"mbedtls/ssl.h",             "-lmbedtls -lmbedx509 -lmbedcrypto"},
    {"mbedtls/net_sockets.h",     "-lmbedtls -lmbedx509 -lmbedcrypto"},
    {"mbedtls/entropy.h",         "-lmbedtls -lmbedx509 -lmbedcrypto"},
    {"gnutls/gnutls.h",           "-lgnutls"},
    {"sodium.h",                  "-lsodium"},
    {"gcrypt.h",                  "-lgcrypt"},
    /* HTTP */
    {"curl/curl.h",               "-lcurl"},
    {"microhttpd.h",              "-lmicrohttpd"},
    /* Event loops / async I/O */
    {"event.h",                   "-levent"},
    {"event2/event.h",            "-levent"},
    {"event2/bufferevent.h",      "-levent"},
    {"event2/thread.h",           "-levent_pthreads -levent"},
    {"ev.h",                      "-lev"},
    {"uv.h",                      "-luv"},
    /* Databases */
    {"sqlite3.h",                 "-lsqlite3"},
    {"mysql/mysql.h",             "-lmysqlclient"},
    {"libpq-fe.h",                "-lpq"},
    {"redis/hiredis.h",           "-lhiredis"},
    {"hiredis/hiredis.h",         "-lhiredis"},
    {"lmdb.h",                    "-llmdb"},
    {"leveldb/c.h",               "-lleveldb"},
    {"rocksdb/c.h",               "-lrocksdb"},
    /* Compression */
    {"zlib.h",                    "-lz"},
    {"bzlib.h",                   "-lbz2"},
    {"bz2.h",                     "-lbz2"},
    {"lzma.h",                    "-llzma"},
    {"lz4.h",                     "-llz4"},
    {"lz4frame.h",                "-llz4"},
    {"lz4hc.h",                   "-llz4"},
    {"zstd.h",                    "-lzstd"},
    {"snappy-c.h",                "-lsnappy"},
    {"blosc.h",                   "-lblosc"},
    /* Data formats */
    {"json-c/json.h",             "-ljson-c"},
    {"jansson.h",                 "-ljansson"},
    {"cjson/cJSON.h",             "-lcjson"},
    {"cJSON.h",                   "-lcjson"},
    {"yaml.h",                    "-lyaml"},
    {"libxml/parser.h",           "-lxml2"},
    {"libxml/tree.h",             "-lxml2"},
    {"libxml/xpath.h",            "-lxml2"},
    {"libxml/xmlreader.h",        "-lxml2"},
    {"expat.h",                   "-lexpat"},
    {"msgpack.h",                 "-lmsgpack"},
    {"cbor.h",                    "-lcbor"},
    {"toml.h",                    "-ltoml"},
    /* UUID */
    {"uuid/uuid.h",               "-luuid"},
    /* Regex */
    {"pcre.h",                    "-lpcre"},
    {"pcre2.h",                   "-lpcre2-8"},
    /* Readline / editline */
    {"readline/readline.h",       "-lreadline"},
    {"readline/history.h",        "-lreadline"},
    {"editline/readline.h",       "-ledit"},
    /* SDL */
    {"SDL.h",                     "-lSDL"},
    {"SDL/SDL.h",                 "-lSDL"},
    {"SDL2/SDL.h",                "-lSDL2"},
    {"SDL2/SDL_image.h",          "-lSDL2_image"},
    {"SDL2/SDL_mixer.h",          "-lSDL2_mixer"},
    {"SDL2/SDL_ttf.h",            "-lSDL2_ttf"},
    {"SDL2/SDL_net.h",            "-lSDL2_net"},
    {"SDL2/SDL_gfx.h",            "-lSDL2_gfx"},
    {"SDL3/SDL.h",                "-lSDL3"},
    /* OpenGL */
    {"GL/gl.h",                   "-lGL"},
    {"GL/glu.h",                  "-lGL -lGLU"},
    {"GL/glut.h",                 "-lGL -lGLU -lglut"},
    {"GL/glew.h",                 "-lGL -lGLEW"},
    {"GL/freeglut.h",             "-lGL -lGLU -lglut"},
    {"GLFW/glfw3.h",              "-lglfw"},
    {"OpenGL/gl.h",               "-framework OpenGL"},
    {"OpenGL/glu.h",              "-framework OpenGL"},
    {"OpenGL/glut.h",             "-framework OpenGL -framework GLUT"},
    {"GLUT/glut.h",               "-framework OpenGL -framework GLUT"},
    {"vulkan/vulkan.h",           "-lvulkan"},
    /* X11 */
    {"X11/Xlib.h",                "-lX11"},
    {"X11/Xutil.h",               "-lX11"},
    {"X11/keysym.h",              "-lX11"},
    {"X11/extensions/Xrender.h",  "-lX11 -lXrender"},
    {"X11/Xft/Xft.h",             "-lX11 -lXft"},
    {"X11/extensions/Xrandr.h",   "-lX11 -lXrandr"},
    {"xcb/xcb.h",                 "-lxcb"},
    /* Cairo / Pango */
    {"cairo/cairo.h",             "-lcairo"},
    {"cairo.h",                   "-lcairo"},
    {"cairo/cairo-xlib.h",        "-lcairo -lX11"},
    {"pango/pango.h",             "-lpango-1.0"},
    {"pangocairo.h",              "-lpangocairo-1.0 -lpango-1.0 -lcairo"},
    /* Images */
    {"png.h",                     "-lpng"},
    {"jpeglib.h",                 "-ljpeg"},
    {"tiff.h",                    "-ltiff"},
    {"webp/decode.h",             "-lwebp"},
    {"webp/encode.h",             "-lwebp"},
    {"gif_lib.h",                 "-lgif"},
    {"MagickWand/MagickWand.h",   "-lMagickWand -lMagickCore"},
    /* Fonts */
    {"ft2build.h",                "-lfreetype"},
    {"freetype/freetype.h",       "-lfreetype"},
    {"fontconfig/fontconfig.h",   "-lfontconfig"},
    /* Audio */
    {"portaudio.h",               "-lportaudio"},
    {"alsa/asoundlib.h",          "-lasound"},
    {"sndfile.h",                 "-lsndfile"},
    {"ao/ao.h",                   "-lao"},
    {"mpg123.h",                  "-lmpg123"},
    {"vorbis/codec.h",            "-lvorbis"},
    {"vorbis/vorbisfile.h",       "-lvorbis -lvorbisfile"},
    {"opus/opus.h",               "-lopus"},
    {"FLAC/stream_decoder.h",     "-lFLAC"},
    {"FLAC/stream_encoder.h",     "-lFLAC"},
    {"fftw3.h",                   "-lfftw3"},
    /* Math / scientific */
    {"gmp.h",                     "-lgmp"},
    {"mpfr.h",                    "-lgmp -lmpfr"},
    {"cblas.h",                   "-lblas"},
    {"lapacke.h",                 "-llapack -lblas"},
    {"gsl/gsl_math.h",            "-lgsl -lgslcblas -lm"},
    {"gsl/gsl_rng.h",             "-lgsl -lgslcblas -lm"},
    /* Filesystem / FUSE */
    {"fuse/fuse.h",               "-lfuse"},
    {"fuse3/fuse.h",              "-lfuse3"},
    /* Scripting */
    {"lua.h",                     "-llua"},
    {"lua5.4/lua.h",              "-llua5.4"},
    {"lua5.3/lua.h",              "-llua5.3"},
    {"lua5.2/lua.h",              "-llua5.2"},
    {"lua5.1/lua.h",              "-llua5.1"},
    {"lauxlib.h",                 "-llua"},
    {"lualib.h",                  "-llua"},
    /* Networking extras */
    {"zmq.h",                     "-lzmq"},
    {"nanomsg/nn.h",              "-lnanomsg"},
    {"nng/nng.h",                 "-lnng"},
    /* Game / multimedia */
    {"raylib.h",                  "-lraylib"},
    {"allegro5/allegro.h",        "-lallegro"},
    {"allegro5/allegro_audio.h",  "-lallegro_audio"},
    /* Geo / projection */
    {"proj_api.h",                "-lproj"},
    {"proj.h",                    "-lproj"},
    /* LDAP */
    {"ldap.h",                    "-lldap"},
    /* Security */
    {"sys/capability.h",          "-lcap"},
    {"cap.h",                     "-lcap"},
    /* GLib */
    {"glib.h",                    "-lglib-2.0"},
    {"glib-2.0/glib.h",           "-lglib-2.0"},
    /* IPC */
    {"glib-2.0/gio/gio.h",        "-lgio-2.0 -lglib-2.0"},
};
#define LIB_MAP_N ((int)(sizeof(LIB_MAP)/sizeof(LIB_MAP[0])))

/* Returns 1 if -lfoo already appears as a whole word inside flags string */
static int flag_in_str(const char *flags, const char *flag) {
    int flen = (int)strlen(flag);
    const char *p = flags;
    while ((p = strstr(p, flag)) != NULL) {
        int before = (p == flags) || (p[-1] == ' ');
        int after  = (p[flen] == '\0') || (p[flen] == ' ');
        if (before && after) return 1;
        p++;
    }
    return 0;
}

/* Scan every .c and .h file in app->files for #include <...> headers
   and append the required -l flags (deduplicated) into out[outsz]. */
static void collect_libs(App *app, char *out, int outsz) {
    out[0] = '\0';
    for (int fi = 0; fi < app->file_count; fi++) {
        const char *nm  = app->files[fi].filename;
        const char *dot = strrchr(nm, '.');
        if (!dot) continue;
        if (strcmp(dot, ".c") != 0 && strcmp(dot, ".h") != 0) continue;
        if (app->files[fi].binary || app->files[fi].unreadable) continue;

        FILE *f = fopen(app->files[fi].path, "r");
        if (!f) continue;

        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] != '#') continue;
            const char *p = line + 1;
            while (*p == ' ' || *p == '\t') p++;
            if (strncmp(p, "include", 7) != 0) continue;
            p += 7;
            while (*p == ' ' || *p == '\t') p++;
            if (*p != '<') continue;
            p++;
            char hdr[128]; int hi = 0;
            while (*p && *p != '>' && hi < (int)sizeof(hdr) - 1) hdr[hi++] = *p++;
            hdr[hi] = '\0';
            if (*p != '>') continue;

            for (int mi = 0; mi < LIB_MAP_N; mi++) {
                if (strcmp(hdr, LIB_MAP[mi].hdr) != 0) continue;
                char tmp[256];
                strncpy(tmp, LIB_MAP[mi].flags, sizeof(tmp) - 1);
                tmp[sizeof(tmp) - 1] = '\0';
                char *lib_save = NULL;
                char *tok = strtok_r(tmp, " ", &lib_save);
                while (tok) {
                    if (!flag_in_str(out, tok)) {
                        int olen = (int)strlen(out);
                        if (olen + (int)strlen(tok) + 2 < outsz) {
                            if (olen > 0) out[olen++] = ' ';
                            strcpy(out + olen, tok);
                        }
                    }
                    tok = strtok_r(NULL, " ", &lib_save);
                }
                break;
            }
        }
        fclose(f);
    }
}

static int run_capture(const char *cmd, Content *out) {
    content_free(out);
    FILE *p = popen(cmd, "r");
    if (!p) {
        content_append_line_raw(out, "[error: popen failed]");
        return -1;
    }
    char line[1024];
    while (fgets(line, sizeof(line), p)) {
        line[strcspn(line, "\r\n")] = '\0';
        content_append_line_raw(out, line);
    }
    int st = pclose(p);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static void run_build(App *app, const char *cmd) {
    attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
    mvhline(STATUS_Y, 0, ' ', COLS);
    mvprintw(STATUS_Y, 1, " Building...");
    attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
    refresh();

    int exit_code = run_capture(cmd, &app->compile_out);
    app->compile_ok = (exit_code == 0);
    snprintf(app->compile_status, sizeof(app->compile_status),
             exit_code == 0 ? "Build OK" : "Build failed (exit %d)", exit_code);
    app->mode        = MODE_COMPILE_OUT;
    app->compile_out_off = 0;
}

static void do_build(App *app) {
    if (app->content_dirty && app->open_file >= 0)
        save_file(app);
    app->compile_prev_mode = app->mode;

    /* 1. Makefile / makefile / GNUmakefile → make */
    char mf[META_PATH_LEN + 20];
    struct stat bst;
    static const char *mk_names[] = { "Makefile", "makefile", "GNUmakefile" };
    for (int i = 0; i < 3; i++) {
        snprintf(mf, sizeof(mf), "%s/%s", app->scan_dir_path, mk_names[i]);
        if (stat(mf, &bst) == 0) {
            char qdir[META_PATH_LEN * 4 + 4];
            shell_quote(app->scan_dir_path, qdir, sizeof(qdir));
            char cmd[META_PATH_LEN * 4 + 32];
            snprintf(cmd, sizeof(cmd), "make -C %s 2>&1", qdir);
            run_build(app, cmd);
            return;
        }
    }

    /* 2. No Makefile — collect all .c files + auto-detect libraries */
    char libs[1024] = "";
    collect_libs(app, libs, sizeof(libs));

    int   c_count  = 0;
    size_t files_sz = 16;
    for (int fi = 0; fi < app->file_count; fi++) {
        const char *dot = strrchr(app->files[fi].filename, '.');
        if (dot && strcmp(dot, ".c") == 0 &&
            !app->files[fi].binary && !app->files[fi].unreadable) {
            c_count++;
            files_sz += strlen(app->files[fi].path) * 4 + 8; /* worst-case shell_quote */
        }
    }

    if (c_count == 0) {
        content_free(&app->compile_out);
        content_append_line_raw(&app->compile_out,
            "No .c files found and no Makefile present.");
        app->compile_ok     = 0;
        app->compile_out_off = 0;
        snprintf(app->compile_status, sizeof(app->compile_status), "No build target");
        app->mode = MODE_COMPILE_OUT;
        return;
    }

    /* Output binary name = last path component of scan dir */
    const char *bname = strrchr(app->scan_dir_path, '/');
    bname = (bname && bname[1]) ? bname + 1 : app->scan_dir_path;
    if (!bname[0] || strcmp(bname, ".") == 0) bname = "output";

    /* Build shell-quoted file list */
    char *files_str = malloc(files_sz);
    if (!files_str) return;
    files_str[0] = '\0';
    for (int fi = 0; fi < app->file_count; fi++) {
        const char *dot = strrchr(app->files[fi].filename, '.');
        if (!dot || strcmp(dot, ".c") != 0) continue;
        if (app->files[fi].binary || app->files[fi].unreadable) continue;
        char qp[META_PATH_LEN * 4 + 4];
        shell_quote(app->files[fi].path, qp, sizeof(qp));
        size_t rem = files_sz - strlen(files_str) - 1;
        strncat(files_str, " ",  rem); rem--;
        strncat(files_str, qp, files_sz - strlen(files_str) - 1);
    }

    /* Shell-quoted output path: scan_dir_path/bname */
    char out_path[META_PATH_LEN * 2];
    snprintf(out_path, sizeof(out_path), "%s/%s", app->scan_dir_path, bname);
    char qout[META_PATH_LEN * 8 + 4];
    shell_quote(out_path, qout, sizeof(qout));

    /* Build gcc command */
    size_t cmd_sz = files_sz + sizeof(qout) + strlen(libs) + 64;
    char  *cmd    = malloc(cmd_sz);
    if (!cmd) { free(files_str); return; }
    snprintf(cmd, cmd_sz,
             "gcc -Wall -Wextra -g%s -o %s%s%s 2>&1",
             files_str, qout,
             libs[0] ? " " : "", libs);
    free(files_str);

    run_build(app, cmd);
    free(cmd);
}

static void handle_compile_out(App *app, int key) {
    int view_h = rpanel_h(app) - 2;
    int total  = app->compile_out.line_count;

    switch (key) {
    case 27: /* Esc — return to previous mode */
        app->mode = app->compile_prev_mode;
        break;

    case KEY_UP:   case 'k':
        if (app->compile_out_off > 0) app->compile_out_off--;
        break;

    case KEY_DOWN: case 'j':
        if (app->compile_out_off + view_h < total) app->compile_out_off++;
        break;

    case KEY_PPAGE: case 'u':
        app->compile_out_off -= view_h;
        if (app->compile_out_off < 0) app->compile_out_off = 0;
        break;

    case KEY_NPAGE: case 'd':
        app->compile_out_off += view_h;
        if (app->compile_out_off + view_h > total)
            app->compile_out_off = total - view_h;
        if (app->compile_out_off < 0) app->compile_out_off = 0;
        break;

    case 'g': case KEY_HOME:
        app->compile_out_off = 0;
        break;

    case 'G': case KEY_END:
        app->compile_out_off = total - view_h;
        if (app->compile_out_off < 0) app->compile_out_off = 0;
        break;

    case 2: /* Ctrl+B — rebuild */
        do_build(app);
        break;

    case 15: /* Ctrl+O — command panel */
        toggle_term(app);
        break;
    }
}

/* ---------------------------------------------------------------
   Key handlers
--------------------------------------------------------------- */

/* Tree view navigation (no query active) */
static void handle_list_tree(App *app, int key) {
    int view_h = PANEL_H - META_PANEL_H - 1;

    switch (key) {
    case KEY_UP:
    case 'k':
        if (app->tree_sel > 0) {
            app->tree_sel--;
            if (app->tree_sel < app->tree_offset)
                app->tree_offset = app->tree_sel;
        }
        break;

    case KEY_DOWN:
    case 'j':
        if (app->tree_sel < app->tree_view_count - 1) {
            app->tree_sel++;
            if (app->tree_sel >= app->tree_offset + view_h)
                app->tree_offset = app->tree_sel - view_h + 1;
        }
        break;

    case KEY_RIGHT:
    case '\n':
    case '\r': {
        if (app->tree_view_count == 0) break;
        int vi    = app->tree_view[app->tree_sel];
        TreeNode *n = &app->tree_all[vi];
        if (n->kind == NODE_DIR) {
            /* Trigger on-demand load if this is a boundary dir not yet loaded */
            if (!n->expanded && is_boundary_dir(app, n->subdir) &&
                !subdir_is_loaded(app, n->subdir)) {
                start_subdir_scan(app, n->subdir);
            }
            n->expanded = !n->expanded;
            rebuild_view(app);
            /* Keep cursor on the same dir node */
            for (int i = 0; i < app->tree_view_count; i++) {
                if (app->tree_view[i] == vi) {
                    app->tree_sel = i;
                    break;
                }
            }
            if (app->tree_sel >= app->tree_view_count)
                app->tree_sel = app->tree_view_count - 1;
            /* Clamp offset */
            if (app->tree_sel < app->tree_offset)
                app->tree_offset = app->tree_sel;
            if (app->tree_sel >= app->tree_offset + view_h)
                app->tree_offset = app->tree_sel - view_h + 1;
        } else if (n->file_idx >= 0) {
            load_content(app, n->file_idx);
            app->highlight_word[0] = '\0';
            app->mode = MODE_CONTENT;
        }
        break;
    }

    case KEY_LEFT: {
        if (app->tree_view_count == 0) break;
        int vi    = app->tree_view[app->tree_sel];
        TreeNode *n = &app->tree_all[vi];
        if (n->kind == NODE_DIR && n->expanded) {
            n->expanded = 0;
            rebuild_view(app);
            for (int i = 0; i < app->tree_view_count; i++) {
                if (app->tree_view[i] == vi) { app->tree_sel = i; break; }
            }
            if (app->tree_sel < app->tree_offset)
                app->tree_offset = app->tree_sel;
        } else if (n->depth > 0) {
            for (int i = app->tree_sel - 1; i >= 0; i--) {
                int pvi = app->tree_view[i];
                if (app->tree_all[pvi].kind == NODE_DIR &&
                    app->tree_all[pvi].depth == n->depth - 1) {
                    app->tree_sel = i;
                    if (app->tree_sel < app->tree_offset)
                        app->tree_offset = app->tree_sel;
                    break;
                }
            }
        }
        break;
    }

    case '/':
        app->mode = MODE_SEARCH;
        curs_set(1);
        break;

    case 14: /* Ctrl+N — new file */
        enter_new_file_mode(app);
        break;

    case 4: { /* Ctrl+D — delete selected file */
        if (app->tree_view_count > 0) {
            int vi = app->tree_view[app->tree_sel];
            const TreeNode *n = &app->tree_all[vi];
            if (n->kind == NODE_FILE && n->file_idx >= 0)
                enter_confirm_del(app, n->file_idx, MODE_LIST);
        }
        break;
    }

    case 20: /* Ctrl+T — filetype filter */
        init_filter_input(app);
        app->mode = MODE_FILTER;
        break;

    case 7: /* Ctrl+G — go to directory */
        app->goto_buf[0] = '\0';
        app->goto_len    = 0;
        app->mode        = MODE_GOTO;
        curs_set(1);
        break;

    case 12: /* Ctrl+L — toggle library / CWD */
        if (app->lib_dir[0]) {
            app->in_lib_view ^= 1;
            rescan(app);
        }
        break;

    case 18: /* Ctrl+R — reload directory */
        rescan(app);
        break;

    case '?':
        app->help_prev_mode = MODE_LIST;
        app->help_offset = 0;
        app->mode = MODE_HELP;
        break;

    case 15: /* Ctrl+O — toggle command panel */
        toggle_term(app);
        break;

    case 25: /* Ctrl+Y — toggle notes panel */
        ensure_vault(app);
        if (app->vault) {
            app->notes_sel    = 0;
            app->notes_offset = 0;
            rebuild_notes_view(app);
            app->mode = MODE_NOTES;
        }
        break;

    case 'q':
    case 'Q':
        app->quit = 1;
        break;

    case KEY_MOUSE: {
        MEVENT ev;
        if (getmouse(&ev) == OK) {
            if (ev.bstate & BUTTON4_PRESSED) {
                if (app->tree_sel > 0) {
                    app->tree_sel--;
                    if (app->tree_sel < app->tree_offset)
                        app->tree_offset = app->tree_sel;
                }
            } else if (BUTTON_PRESS(ev.bstate, 5)) {
                if (app->tree_sel < app->tree_view_count - 1) {
                    app->tree_sel++;
                    if (app->tree_sel >= app->tree_offset + view_h)
                        app->tree_offset = app->tree_sel - view_h + 1;
                }
            }
        } else {
            if (app->tree_sel < app->tree_view_count - 1) {
                app->tree_sel++;
                if (app->tree_sel >= app->tree_offset + view_h)
                    app->tree_offset = app->tree_sel - view_h + 1;
            }
        }
        break;
    }

    default:
        if (key >= 32 && key < 127) {
            app->mode = MODE_SEARCH;
            curs_set(1);
            if (app->query_len < (int)sizeof(app->query) - 2) {
                app->query[app->query_len++] = (char)key;
                app->query[app->query_len]   = '\0';
                if (!app->scan_full) {
                    app->scan_full = 1;
                    start_scan(app, -1);
                }
                update_search(app);
            }
        }
        break;
    }
}

/* Filtered list navigation (query active) */
static void handle_list_filtered(App *app, int key) {
    int view_h = PANEL_H - META_PANEL_H - 1;

    switch (key) {
    case KEY_UP:
    case 'k':
        if (app->list_sel > 0) {
            app->list_sel--;
            if (app->list_sel < app->list_offset)
                app->list_offset = app->list_sel;
        }
        break;

    case KEY_DOWN:
    case 'j':
        if (app->list_sel < app->filtered_count - 1) {
            app->list_sel++;
            if (app->list_sel >= app->list_offset + view_h)
                app->list_offset = app->list_sel - view_h + 1;
        }
        break;

    case KEY_RIGHT:
    case '\n':
    case '\r':
        if (app->filtered_count > 0) {
            load_content(app, app->filtered[app->list_sel]);
            strncpy(app->highlight_word, app->query, sizeof(app->highlight_word) - 1);
            app->highlight_word[sizeof(app->highlight_word) - 1] = '\0';
            char *sp = strchr(app->highlight_word, ' ');
            if (sp) *sp = '\0';
            app->mode = MODE_CONTENT;
        }
        break;

    case '/':
        app->mode = MODE_SEARCH;
        curs_set(1);
        break;

    case 14: /* Ctrl+N — new file */
        enter_new_file_mode(app);
        break;

    case 4: /* Ctrl+D — delete selected file */
        if (app->filtered_count > 0)
            enter_confirm_del(app, app->filtered[app->list_sel], MODE_LIST);
        break;

    case 20: /* Ctrl+T — filetype filter */
        init_filter_input(app);
        app->mode = MODE_FILTER;
        break;

    case 7: /* Ctrl+G — go to directory */
        app->goto_buf[0] = '\0';
        app->goto_len    = 0;
        app->mode        = MODE_GOTO;
        curs_set(1);
        break;

    case 12: /* Ctrl+L — toggle library / CWD */
        if (app->lib_dir[0]) {
            app->in_lib_view ^= 1;
            rescan(app);
        }
        break;

    case 18: /* Ctrl+R — reload directory */
        rescan(app);
        break;

    case 15: /* Ctrl+O — toggle command panel */
        toggle_term(app);
        break;

    case 25: /* Ctrl+Y — toggle notes panel */
        ensure_vault(app);
        if (app->vault) {
            app->notes_sel    = 0;
            app->notes_offset = 0;
            rebuild_notes_view(app);
            app->mode = MODE_NOTES;
        }
        break;

    case '?':
        app->help_prev_mode = MODE_LIST;
        app->help_offset = 0;
        app->mode = MODE_HELP;
        break;

    case 'q':
    case 'Q':
        app->quit = 1;
        break;

    case KEY_MOUSE: {
        MEVENT ev;
        if (getmouse(&ev) == OK) {
            if (ev.bstate & BUTTON4_PRESSED) {
                if (app->list_sel > 0) {
                    app->list_sel--;
                    if (app->list_sel < app->list_offset)
                        app->list_offset = app->list_sel;
                }
            } else if (BUTTON_PRESS(ev.bstate, 5)) {
                if (app->list_sel < app->filtered_count - 1) {
                    app->list_sel++;
                    if (app->list_sel >= app->list_offset + view_h)
                        app->list_offset = app->list_sel - view_h + 1;
                }
            }
        } else {
            if (app->list_sel < app->filtered_count - 1) {
                app->list_sel++;
                if (app->list_sel >= app->list_offset + view_h)
                    app->list_offset = app->list_sel - view_h + 1;
            }
        }
        break;
    }

    default:
        if (key >= 32 && key < 127) {
            app->mode = MODE_SEARCH;
            curs_set(1);
            if (app->query_len < (int)sizeof(app->query) - 2) {
                app->query[app->query_len++] = (char)key;
                app->query[app->query_len]   = '\0';
                if (!app->scan_full) {
                    app->scan_full = 1;
                    start_scan(app, -1);
                }
                update_search(app);
            }
        }
        break;
    }
}

/* Dispatcher */
static void handle_list(App *app, int key) {
    if (app->query[0] == '\0')
        handle_list_tree(app, key);
    else
        handle_list_filtered(app, key);
}

/* ---------------------------------------------------------------
   Notes panel key handler
--------------------------------------------------------------- */

/* Wipe decrypted content of all password-protected notes from memory.
   Called when leaving the notes panel so the next visit requires re-auth. */
static void vault_lock_pw_notes(App *app) {
    NoteVault *v = app->vault;
    if (!v) return;
    for (int i = 0; i < v->note_count; i++) {
        Note *n = &v->notes[i];
        if (n->has_pw && n->content) {
            sodium_memzero(n->content, strlen(n->content));
            free(n->content);
            n->content = NULL;
        }
    }
    /* If the open note was password-protected, close it */
    if (app->open_note >= 0 && v->notes[app->open_note].has_pw) {
        content_free(&app->content);
        app->open_note = -1;
        app->note_active_pw[0] = '\0';
    }
}

static void ensure_vault(App *app) {
    if (app->vault) return;
    app->vault = calloc(1, sizeof(NoteVault));
    if (!app->vault) return;
    app->open_note = -1;
    vault_open(app->vault);
    rebuild_notes_view(app);
}

static void handle_note_new_cat(App *app, int key) {
    NoteVault *v = app->vault;
    if (key == 27) { /* Esc */
        app->mode = MODE_NOTES;
        curs_set(0);
        return;
    }
    if (key == '\n' || key == '\r') {
        if (app->note_new_cat_len > 0 && v) {
            vault_add_cat(v, app->note_new_cat_buf);
            vault_save(v);
            rebuild_notes_view(app);
        }
        app->mode = MODE_NOTES;
        curs_set(0);
        return;
    }
    if ((key == KEY_BACKSPACE || key == 127 || key == '\b') && app->note_new_cat_len > 0) {
        app->note_new_cat_buf[--app->note_new_cat_len] = '\0';
        return;
    }
    if (key >= 32 && key < 127 && app->note_new_cat_len < NOTE_CAT_LEN - 1) {
        app->note_new_cat_buf[app->note_new_cat_len++] = (char)key;
        app->note_new_cat_buf[app->note_new_cat_len]   = '\0';
    }
}

static void handle_notes(App *app, int key) {
    int view_h = PANEL_H / 2 - 2;
    NoteVault *v = app->vault;
    int nc = app->notes_view_count;

    switch (key) {
    case KEY_UP: case 'k':
        if (app->notes_sel > 0) {
            app->notes_sel--;
            if (app->notes_sel < app->notes_offset)
                app->notes_offset = app->notes_sel;
        }
        break;

    case KEY_DOWN: case 'j':
        if (app->notes_sel < nc - 1) {
            app->notes_sel++;
            if (app->notes_sel >= app->notes_offset + view_h)
                app->notes_offset = app->notes_sel - view_h + 1;
        }
        break;

    case KEY_RIGHT: case '\n': case '\r': {
        if (!v || nc == 0) break;
        NViewItem item = app->notes_view[app->notes_sel];
        if (item.kind == NVIEW_CAT) break;
        int ni = item.idx;
        Note *n = &v->notes[ni];

        if (n->has_pw && !n->content) {
            /* Ask for password */
            app->note_pw_idx     = ni;
            app->note_pw_buf[0]  = '\0';
            app->note_pw_len     = 0;
            app->note_pw_visible = 0;
            app->mode            = MODE_NOTE_PW;
            curs_set(1);
        } else {
            load_note_content(app, ni);
            app->note_active_pw[0] = '\0';
            app->mode = MODE_NOTES;
        }
        break;
    }

    case 'e': /* enter edit mode for open note */
        if (app->open_note >= 0) {
            app->cursor_row = app->content.line_count > 0
                              ? app->content.line_count - 1 : 0;
            app->cursor_col = 0;
            sel_clear(app);
            app->mode = MODE_EDIT;
            curs_set(1);
        }
        break;

    case 25: /* Ctrl+Y — toggle back to file tree */
        vault_lock_pw_notes(app);
        app->open_note = -1;
        app->mode = MODE_LIST;
        break;

    case 14: { /* Ctrl+N — new note */
        if (!v) break;
        app->note_new_title[0]  = '\0';
        app->note_new_title_len = 0;
        app->note_new_cat_sel   = 0;
        app->mode = MODE_NOTE_NEW;
        curs_set(1);
        break;
    }

    case 4: { /* Ctrl+D — delete selected note */
        if (!v || nc == 0) break;
        NViewItem item = app->notes_view[app->notes_sel];
        if (item.kind != NVIEW_NOTE) break;
        vault_delete_note(v, item.idx);
        vault_save(v);
        if (app->open_note == item.idx) app->open_note = -1;
        else if (app->open_note > item.idx) app->open_note--;
        rebuild_notes_view(app);
        if (app->notes_sel >= app->notes_view_count && app->notes_sel > 0)
            app->notes_sel--;
        break;
    }

    case '/': case 6: /* Ctrl+F */
        app->note_sq[0]     = '\0';
        app->note_sq_len    = 0;
        app->note_hit_count = 0;
        app->note_hit_sel   = 0;
        app->note_hit_offset = 0;
        app->mode = MODE_NOTE_SEARCH;
        curs_set(1);
        break;

    case 11: /* Ctrl+K — new category/folder */
        if (!v) break;
        app->note_new_cat_buf[0] = '\0';
        app->note_new_cat_len    = 0;
        app->mode = MODE_NOTE_NEW_CAT;
        curs_set(1);
        break;

    case 'q': case 'Q':
        app->quit = 1;
        break;

    case '?':
        app->help_prev_mode = MODE_NOTES;
        app->help_offset = 0;
        app->mode = MODE_HELP;
        break;

    case KEY_MOUSE: {
        MEVENT ev;
        if (getmouse(&ev) == OK) {
            if (ev.bstate & BUTTON4_PRESSED) {
                if (app->notes_sel > 0) {
                    app->notes_sel--;
                    if (app->notes_sel < app->notes_offset)
                        app->notes_offset = app->notes_sel;
                }
            } else if (BUTTON_PRESS(ev.bstate, 5)) {
                if (app->notes_sel < nc - 1) {
                    app->notes_sel++;
                    if (app->notes_sel >= app->notes_offset + view_h)
                        app->notes_offset = app->notes_sel - view_h + 1;
                }
            }
        } else {
            if (app->notes_sel < nc - 1) {
                app->notes_sel++;
                if (app->notes_sel >= app->notes_offset + view_h)
                    app->notes_offset = app->notes_sel - view_h + 1;
            }
        }
        break;
    }

    default: break;
    }
}

/* ---------------------------------------------------------------
   Password prompt for locked notes
--------------------------------------------------------------- */
static void handle_note_pw(App *app, int key) {
    switch (key) {
    case '\n': case '\r': {
        /* Try decryption */
        int rc = vault_decrypt_note(app->vault, app->note_pw_idx,
                                    app->note_pw_buf);
        if (rc == 0) {
            strncpy(app->note_active_pw, app->note_pw_buf,
                    sizeof(app->note_active_pw) - 1);
            sodium_memzero(app->note_pw_buf, sizeof(app->note_pw_buf));
            app->note_pw_len = 0;
            load_note_content(app, app->note_pw_idx);
            app->mode = MODE_NOTES;
            curs_set(0);
        } else {
            /* Wrong password: clear and let user retry */
            sodium_memzero(app->note_pw_buf, sizeof(app->note_pw_buf));
            app->note_pw_len = 0;
        }
        break;
    }

    case 27: /* Esc — cancel */
        sodium_memzero(app->note_pw_buf, sizeof(app->note_pw_buf));
        app->note_pw_len = 0;
        app->mode = MODE_NOTES;
        curs_set(0);
        break;

    case KEY_BACKSPACE: case 127: case '\b':
        if (app->note_pw_len > 0) {
            app->note_pw_buf[--app->note_pw_len] = '\0';
        }
        break;

    default:
        if (key >= 32 && key < 127 &&
            app->note_pw_len < (int)sizeof(app->note_pw_buf) - 2) {
            app->note_pw_buf[app->note_pw_len++] = (char)key;
            app->note_pw_buf[app->note_pw_len]   = '\0';
        }
        break;
    }
}

/* ---------------------------------------------------------------
   New note title prompt
--------------------------------------------------------------- */
/* Select + open a note in notes panel */
static void note_select_and_open(App *app, int idx) {
    rebuild_notes_view(app);
    for (int i = 0; i < app->notes_view_count; i++) {
        if (app->notes_view[i].kind == NVIEW_NOTE &&
            app->notes_view[i].idx == idx) {
            app->notes_sel = i;
            break;
        }
    }
    load_note_content(app, idx);
    app->cursor_row = 0;
    app->cursor_col = 0;
    sel_clear(app);
    app->mode = MODE_NOTES;
    curs_set(0);
}

static void handle_note_new(App *app, int key) {
    NoteVault *v = app->vault;
    if (!v) { app->mode = MODE_NOTES; return; }

    /* --- step 0: title input --- */
    if (app->note_new_step == 0) {
        switch (key) {
        case '\n': case '\r':
            if (app->note_new_title_len == 0) {
                app->mode = MODE_NOTES;
                curs_set(0);
                break;
            }
            /* Title entered → ask about password */
            app->note_new_step = 1;
            break;
        case 27:
            app->note_new_title[0] = '\0';
            app->note_new_title_len = 0;
            app->note_new_step = 0;
            app->mode = MODE_NOTES;
            curs_set(0);
            break;
        case KEY_BACKSPACE: case 127: case '\b':
            if (app->note_new_title_len > 0)
                app->note_new_title[--app->note_new_title_len] = '\0';
            break;
        default:
            if (key >= 32 && key < 127 &&
                app->note_new_title_len < NOTE_TITLE_LEN - 2) {
                app->note_new_title[app->note_new_title_len++] = (char)key;
                app->note_new_title[app->note_new_title_len]   = '\0';
            }
            break;
        }
        return;
    }

    /* --- step 1: "Add password? [y/n]" --- */
    if (app->note_new_step == 1) {
        if (key == 'y' || key == 'Y') {
            app->note_new_pw1[0] = '\0'; app->note_new_pw1_len = 0;
            app->note_new_pw2[0] = '\0'; app->note_new_pw2_len = 0;
            app->note_new_step = 2;
        } else if (key == 'n' || key == 'N' || key == '\n' || key == '\r') {
            /* No password — create without password */
            const char *cat_id = (v->cat_count > 0 && app->note_new_cat_sel < v->cat_count)
                                 ? v->cats[app->note_new_cat_sel].id : "";
            int idx = vault_add_note(v, cat_id, app->note_new_title, NOTE_TEXT);
            app->note_new_step = 0;
            if (idx >= 0) {
                vault_encrypt_note(v, idx, NULL);
                vault_save(v);
                app->note_active_pw[0] = '\0';
                note_select_and_open(app, idx);
            } else {
                app->mode = MODE_NOTES; curs_set(0);
            }
        } else if (key == 27) {
            app->note_new_step = 0;
            app->note_new_title[0] = '\0'; app->note_new_title_len = 0;
            app->mode = MODE_NOTES; curs_set(0);
        }
        return;
    }

    /* --- step 2: enter password (first) --- */
    if (app->note_new_step == 2) {
        switch (key) {
        case '\n': case '\r':
            if (app->note_new_pw1_len == 0) {
                /* Blank → back to y/n */
                app->note_new_step = 1;
            } else {
                app->note_new_step = 3;
            }
            break;
        case 27:
            app->note_new_step = 0;
            app->note_new_title[0] = '\0'; app->note_new_title_len = 0;
            sodium_memzero(app->note_new_pw1, sizeof(app->note_new_pw1));
            app->note_new_pw1_len = 0;
            app->mode = MODE_NOTES; curs_set(0);
            break;
        case KEY_BACKSPACE: case 127: case '\b':
            if (app->note_new_pw1_len > 0)
                app->note_new_pw1[--app->note_new_pw1_len] = '\0';
            break;
        default:
            if (key >= 32 && key < 127 && app->note_new_pw1_len < 254) {
                app->note_new_pw1[app->note_new_pw1_len++] = (char)key;
                app->note_new_pw1[app->note_new_pw1_len]   = '\0';
            }
            break;
        }
        return;
    }

    /* --- step 3: confirm password --- */
    if (app->note_new_step == 3) {
        switch (key) {
        case '\n': case '\r': {
            if (strcmp(app->note_new_pw1, app->note_new_pw2) != 0) {
                /* mismatch → back to step 2 */
                sodium_memzero(app->note_new_pw1, sizeof(app->note_new_pw1));
                app->note_new_pw1_len = 0;
                sodium_memzero(app->note_new_pw2, sizeof(app->note_new_pw2));
                app->note_new_pw2_len = 0;
                app->note_new_step = 2;
                break;
            }
            const char *cat_id = (v->cat_count > 0 && app->note_new_cat_sel < v->cat_count)
                                 ? v->cats[app->note_new_cat_sel].id : "";
            int idx = vault_add_note(v, cat_id, app->note_new_title, NOTE_TEXT);
            if (idx >= 0) {
                vault_encrypt_note(v, idx, app->note_new_pw1);
                vault_save(v);
                strncpy(app->note_active_pw, app->note_new_pw1,
                        sizeof(app->note_active_pw) - 1);
            }
            sodium_memzero(app->note_new_pw1, sizeof(app->note_new_pw1));
            app->note_new_pw1_len = 0;
            sodium_memzero(app->note_new_pw2, sizeof(app->note_new_pw2));
            app->note_new_pw2_len = 0;
            app->note_new_step = 0;
            if (idx >= 0) {
                note_select_and_open(app, idx);
            } else {
                app->mode = MODE_NOTES; curs_set(0);
            }
            break;
        }
        case 27:
            app->note_new_step = 0;
            app->note_new_title[0] = '\0'; app->note_new_title_len = 0;
            sodium_memzero(app->note_new_pw1, sizeof(app->note_new_pw1));
            app->note_new_pw1_len = 0;
            sodium_memzero(app->note_new_pw2, sizeof(app->note_new_pw2));
            app->note_new_pw2_len = 0;
            app->mode = MODE_NOTES; curs_set(0);
            break;
        case KEY_BACKSPACE: case 127: case '\b':
            if (app->note_new_pw2_len > 0)
                app->note_new_pw2[--app->note_new_pw2_len] = '\0';
            break;
        default:
            if (key >= 32 && key < 127 && app->note_new_pw2_len < 254) {
                app->note_new_pw2[app->note_new_pw2_len++] = (char)key;
                app->note_new_pw2[app->note_new_pw2_len]   = '\0';
            }
            break;
        }
        return;
    }
}

/* ---------------------------------------------------------------
   Note password change handler (Ctrl+E in edit mode)
   step 0 = enter current password (only when note has_pw)
   step 1 = enter new password (blank → remove password)
   step 2 = confirm new password
--------------------------------------------------------------- */
static void handle_note_set_pw(App *app, int key)
{
    NoteVault *v = app->vault;
    if (!v || app->open_note < 0) { app->mode = MODE_EDIT; curs_set(1); return; }
    Note *n = &v->notes[app->open_note];

    /* helpers: active buf / len based on step */
    char *buf  = app->note_setpw_step == 0 ? app->note_setpw_old
               : app->note_setpw_step == 1 ? app->note_setpw_new1
               :                              app->note_setpw_new2;
    int  *blen = app->note_setpw_step == 0 ? &app->note_setpw_old_len
               : app->note_setpw_step == 1 ? &app->note_setpw_new1_len
               :                              &app->note_setpw_new2_len;

    switch (key) {
    case '\n': case '\r':
        if (app->note_setpw_step == 0) {
            /* verify current password against the one used to decrypt */
            if (strcmp(app->note_setpw_old, app->note_active_pw) != 0) {
                sodium_memzero(app->note_setpw_old, sizeof(app->note_setpw_old));
                app->note_setpw_old_len = 0;
                snprintf(app->note_setpw_msg, sizeof(app->note_setpw_msg),
                         "Yanlis sifre, tekrar dene");
            } else {
                sodium_memzero(app->note_setpw_old, sizeof(app->note_setpw_old));
                app->note_setpw_old_len = 0;
                app->note_setpw_msg[0] = '\0';
                app->note_setpw_step = 1;
            }
        } else if (app->note_setpw_step == 1) {
            if (app->note_setpw_new1_len == 0) {
                /* blank = remove password */
                /* sync editor content to n->content first */
                size_t total = 0;
                for (int i = 0; i < app->content.line_count; i++)
                    total += strlen(app->content.lines[i]) + 1;
                char *text = malloc(total + 1);
                if (text) {
                    size_t pos = 0;
                    for (int i = 0; i < app->content.line_count; i++) {
                        size_t ll = strlen(app->content.lines[i]);
                        memcpy(text + pos, app->content.lines[i], ll);
                        pos += ll;
                        if (i < app->content.line_count - 1) text[pos++] = '\n';
                    }
                    text[pos] = '\0';
                    if (n->content) { sodium_memzero(n->content, strlen(n->content)); free(n->content); }
                    n->content = text;
                }
                vault_clear_note_pw(v, app->open_note);
                vault_save(v);
                app->note_active_pw[0] = '\0';
                app->content_dirty = 0;
                rebuild_notes_view(app);
                app->mode = MODE_EDIT;
                curs_set(1);
            } else {
                app->note_setpw_msg[0] = '\0';
                app->note_setpw_step = 2;
            }
        } else { /* step 2: confirm */
            if (strcmp(app->note_setpw_new1, app->note_setpw_new2) != 0) {
                sodium_memzero(app->note_setpw_new1, sizeof(app->note_setpw_new1));
                app->note_setpw_new1_len = 0;
                sodium_memzero(app->note_setpw_new2, sizeof(app->note_setpw_new2));
                app->note_setpw_new2_len = 0;
                app->note_setpw_step = 1;
                snprintf(app->note_setpw_msg, sizeof(app->note_setpw_msg),
                         "Sifreler eslesmiyor, tekrar gir");
            } else {
                /* sync editor content to n->content */
                size_t total = 0;
                for (int i = 0; i < app->content.line_count; i++)
                    total += strlen(app->content.lines[i]) + 1;
                char *text = malloc(total + 1);
                if (text) {
                    size_t pos = 0;
                    for (int i = 0; i < app->content.line_count; i++) {
                        size_t ll = strlen(app->content.lines[i]);
                        memcpy(text + pos, app->content.lines[i], ll);
                        pos += ll;
                        if (i < app->content.line_count - 1) text[pos++] = '\n';
                    }
                    text[pos] = '\0';
                    if (n->content) { sodium_memzero(n->content, strlen(n->content)); free(n->content); }
                    n->content = text;
                }
                n->has_pw = 1;
                vault_encrypt_note(v, app->open_note, app->note_setpw_new1);
                vault_save(v);
                strncpy(app->note_active_pw, app->note_setpw_new1,
                        sizeof(app->note_active_pw) - 1);
                sodium_memzero(app->note_setpw_new1, sizeof(app->note_setpw_new1));
                app->note_setpw_new1_len = 0;
                sodium_memzero(app->note_setpw_new2, sizeof(app->note_setpw_new2));
                app->note_setpw_new2_len = 0;
                app->content_dirty = 0;
                rebuild_notes_view(app);
                app->mode = MODE_EDIT;
                curs_set(1);
            }
        }
        break;

    case 27: /* Esc — cancel */
        sodium_memzero(app->note_setpw_old,  sizeof(app->note_setpw_old));
        app->note_setpw_old_len = 0;
        sodium_memzero(app->note_setpw_new1, sizeof(app->note_setpw_new1));
        app->note_setpw_new1_len = 0;
        sodium_memzero(app->note_setpw_new2, sizeof(app->note_setpw_new2));
        app->note_setpw_new2_len = 0;
        app->note_setpw_msg[0] = '\0';
        app->mode = MODE_EDIT;
        curs_set(1);
        break;

    case KEY_BACKSPACE: case 127: case '\b':
        if (*blen > 0) buf[--(*blen)] = '\0';
        break;

    default:
        if (key >= 32 && key < 127 && *blen < 254) {
            buf[(*blen)++] = (char)key;
            buf[*blen] = '\0';
        }
        break;
    }
    (void)n; /* used above */
}

/* ---------------------------------------------------------------
   Notes search handler
--------------------------------------------------------------- */
static void handle_note_search(App *app, int key) {
    NoteVault *v = app->vault;

    switch (key) {
    case 27: /* Esc — back to notes */
        app->mode = MODE_NOTES;
        curs_set(0);
        break;

    case KEY_UP: case 'k':
        if (app->note_hit_sel > 0) app->note_hit_sel--;
        break;

    case KEY_DOWN: case 'j':
        if (app->note_hit_sel < app->note_hit_count - 1) app->note_hit_sel++;
        break;

    case '\n': case '\r': {
        if (app->note_hit_count == 0 || !v) break;
        NoteHit *h = &app->note_hits[app->note_hit_sel];
        if (!v->notes[h->note_idx].content) break;
        load_note_content(app, h->note_idx);
        /* Scroll to matching line */
        app->content_offset = h->line_no > 1 ? h->line_no - 2 : 0;
        app->mode = MODE_CONTENT;
        curs_set(0);
        break;
    }

    case KEY_BACKSPACE: case 127: case '\b':
        if (app->note_sq_len > 0) {
            app->note_sq[--app->note_sq_len] = '\0';
            if (v && app->note_sq_len > 0)
                app->note_hit_count = vault_search(v, app->note_sq,
                                                   app->note_hits, 512);
            else
                app->note_hit_count = 0;
            app->note_hit_sel = 0;
        }
        break;

    default:
        if (key >= 32 && key < 127 &&
            app->note_sq_len < (int)sizeof(app->note_sq) - 2) {
            app->note_sq[app->note_sq_len++] = (char)key;
            app->note_sq[app->note_sq_len]   = '\0';
            if (v)
                app->note_hit_count = vault_search(v, app->note_sq,
                                                   app->note_hits, 512);
            app->note_hit_sel = 0;
        }
        break;
    }
}

static void handle_content(App *app, int key) {
    int view_h  = rpanel_h(app) - 2;
    /* Backward scan: find the highest content_offset where the last line of
       the file is still visible.  The naive (line_count - view_h) formula
       under-scrolls when lines near the bottom span multiple display rows. */
    int mw = COLS - (LEFT_W + 1) - 2 - LNUM_W;
    if (mw < 1) mw = 1;
    int max_off = 0;
    {
        int disp = 0;
        for (int i = app->content.line_count - 1; i >= 0; i--) {
            int llen = (int)strlen(app->content.lines[i]);
            int nc = (llen == 0) ? 1 : (llen + mw - 1) / mw;
            disp += nc;
            if (disp > view_h) break;
            max_off = i;
        }
    }

    switch (key) {
    case KEY_UP:   case 'k':
        if (app->content_offset > 0) app->content_offset--;
        break;

    case KEY_DOWN: case 'j':
        if (app->content_offset < max_off) app->content_offset++;
        break;

    case KEY_PPAGE: case 'u':
        app->content_offset -= view_h;
        if (app->content_offset < 0) app->content_offset = 0;
        break;

    case KEY_NPAGE: case 'd':
        app->content_offset += view_h;
        if (app->content_offset > max_off) app->content_offset = max_off;
        break;

    case 'g': case KEY_HOME:
        app->content_offset = 0;
        break;

    case 'G': case KEY_END:
        app->content_offset = max_off;
        break;

    case '[':
        for (int i = app->content_offset - 1; i >= 0; i--) {
            if (app->content.lines[i][0] == '#') {
                app->content_offset = i;
                break;
            }
        }
        break;

    case ']':
        for (int i = app->content_offset + 1; i < app->content.line_count; i++) {
            if (app->content.lines[i][0] == '#') {
                app->content_offset = i;
                break;
            }
        }
        break;

    case KEY_LEFT: case 27:
        app->mode = MODE_LIST;
        break;

    case 'e':
        if (app->open_file >= 0) {
            app->cursor_row = app->content_offset;
            app->cursor_col = 0;
            sel_clear(app);
            app->mode = MODE_EDIT;
            curs_set(1);
        }
        break;

    case '/':
        app->mode = MODE_SEARCH;
        curs_set(1);
        break;

    case 4: /* Ctrl+D — delete open file */
        if (app->open_file >= 0)
            enter_confirm_del(app, app->open_file, MODE_CONTENT);
        break;

    case 20: /* Ctrl+T — filetype filter */
        init_filter_input(app);
        app->mode = MODE_FILTER;
        break;

    case 12: /* Ctrl+L — toggle library / CWD */
        if (app->lib_dir[0]) {
            app->in_lib_view ^= 1;
            rescan(app);
            app->mode = MODE_LIST;
        }
        break;

    case 14: /* Ctrl+N — new file */
        enter_new_file_mode(app);
        break;

    case 18: /* Ctrl+R — reload file from disk */
        if (app->open_file >= 0) {
            int saved_offset = app->content_offset;
            load_content(app, app->open_file);
            app->content_offset = saved_offset;
            if (app->content_offset > app->content.line_count - 1)
                app->content_offset = app->content.line_count > 0
                                      ? app->content.line_count - 1 : 0;
        }
        break;

    case 2: /* Ctrl+B — build */
        do_build(app);
        break;

    case 15: /* Ctrl+O — toggle command panel */
        toggle_term(app);
        break;

    case '?':
        app->help_prev_mode = MODE_CONTENT;
        app->help_offset = 0;
        app->mode = MODE_HELP;
        break;

    case 'q': case 'Q':
        app->quit = 1;
        break;

    case KEY_MOUSE: {
        MEVENT ev;
        if (getmouse(&ev) == OK) {
            if (ev.bstate & BUTTON4_PRESSED) {
                if (app->content_offset > 0) app->content_offset--;
            } else if (BUTTON_PRESS(ev.bstate, 5)) {
                if (app->content_offset < max_off) app->content_offset++;
            }
        } else {
            if (app->content_offset < max_off) app->content_offset++;
        }
        break;
    }
    }
}

static void handle_edit(App *app, int key) {
    if (app->save_flash > 0) app->save_flash--;
    app->last_key = key;

    int view_h = rpanel_h(app) - 2;
    const char *cur_line = (app->content.line_count > 0)
                           ? app->content.lines[app->cursor_row] : "";
    int cur_len = (int)strlen(cur_line);

    switch (key) {

    /* Exit edit mode — but only if Esc was pressed alone. If more bytes are
       waiting in the input buffer, this is the head of an unbound escape
       sequence (some terminals emit \033\033[... for Shift+Opt+Arrow), and
       silently exiting edit mode on every unknown shortcut is jarring. */
    case 27: {
        nodelay(stdscr, TRUE);
        int peek = getch();
        if (peek != ERR) {
            /* Drain remainder of the unbound sequence and stay in edit mode. */
            while (getch() != ERR) {}
            nodelay(stdscr, FALSE);
            break;
        }
        nodelay(stdscr, FALSE);
        sel_clear(app);
        app->mode = (app->open_note >= 0) ? MODE_NOTES : MODE_CONTENT;
        curs_set(0);
    } break;

    /* Save */
    case 's' & 0x1F:
        if (app->open_note >= 0) {
            if (save_note(app)) app->save_flash = 3;
        } else {
            if (save_file(app)) app->save_flash = 3;
        }
        break;

    /* Reload file from disk (discards unsaved changes) */
    case 'r' & 0x1F:
        if (app->open_file >= 0) {
            app->cursor_row     = 0;
            app->cursor_col     = 0;
            app->content_offset = 0;
            sel_clear(app);
            load_content(app, app->open_file);
            app->content_dirty = 0;
            app->mode = MODE_CONTENT;
            curs_set(0);
        }
        break;

    /* Build */
    case 'b' & 0x1F:
        do_build(app);
        break;

    /* Command panel */
    case 'o' & 0x1F:
        toggle_term(app);
        break;

    /* Help */
    case KEY_F(1):
        app->help_prev_mode = MODE_EDIT;
        app->help_offset = 0;
        app->mode = MODE_HELP;
        curs_set(0);
        break;

    /* Undo */
    case 'z' & 0x1F:
        undo_pop(app);
        break;

    /* Select all */
    case 'a' & 0x1F:
        edit_select_all(app);
        break;

    /* Cut */
    case 'x' & 0x1F:
        if (app->sel_active) edit_cut_sel(app);
        break;

    /* Copy */
    case 'c' & 0x1F:
        if (app->sel_active) { edit_copy_sel(app); sel_clear(app); }
        break;

    /* Paste */
    case 'v' & 0x1F:
        if (app->sel_active) edit_delete_sel(app);
        edit_paste(app);
        break;

    /* Ctrl+F — open find prompt */
    case 'f' & 0x1F:
        app->mode = MODE_EDIT_FIND;
        /* Keep existing find_query so user can re-run last search */
        app->find_match_count = find_total_matches(app);
        break;

    /* Ctrl+G — find next (works in edit mode too) */
    case 'g' & 0x1F:
        if (app->find_query_len > 0) find_jump_next(app);
        break;

    /* Ctrl+L — select current line (Sublime style) */
    case 'l' & 0x1F:
        edit_select_line(app);
        break;

    /* Ctrl+D — duplicate current line */
    case 'd' & 0x1F:
        edit_duplicate_line(app);
        break;


    /* Ctrl+K — delete current line */
    case 'k' & 0x1F:
        edit_delete_line(app);
        break;

    /* Alt+Up / Alt+Down — move current line up/down */
    case KEY_LINE_MOVE_UP:
        edit_move_line_up(app);
        break;
    case KEY_LINE_MOVE_DN:
        edit_move_line_down(app);
        break;

    /* Cursor movement — clear selection */
    case KEY_UP:
        sel_clear(app); cursor_move_up(app);
        break;
    case KEY_DOWN:
        sel_clear(app); cursor_move_down(app);
        break;
    case KEY_LEFT:
        sel_clear(app); cursor_move_left(app);
        break;
    case KEY_RIGHT:
        sel_clear(app); cursor_move_right(app);
        break;

    /* Shift+Left / Shift+Right — extend selection one char */
    case KEY_SLEFT:
        if (!app->sel_active) sel_start(app);
        cursor_move_left(app);
        break;
    case KEY_SRIGHT:
        if (!app->sel_active) sel_start(app);
        cursor_move_right(app);
        break;

    /* Shift+Up / Shift+Down — extend selection by one line */
    case KEY_SR:    /* ncurses name for shift-up */
        if (!app->sel_active) sel_start(app);
        cursor_move_up(app);
        break;
    case KEY_SF:    /* ncurses name for shift-down */
        if (!app->sel_active) sel_start(app);
        cursor_move_down(app);
        break;

    /* Option+Left/Right — word jump */
    case KEY_WORD_BACK:
        sel_clear(app);
        app->cursor_col = word_boundary_back(cur_line, app->cursor_col);
        break;
    case KEY_WORD_FWD:
        sel_clear(app);
        app->cursor_col = word_boundary_fwd(cur_line, app->cursor_col);
        break;

    /* Shift+Option+Left/Right — word selection */
    case KEY_SEL_WBACK:
        if (!app->sel_active) sel_start(app);
        app->cursor_col = word_boundary_back(cur_line, app->cursor_col);
        break;
    case KEY_SEL_WFWD:
        if (!app->sel_active) sel_start(app);
        app->cursor_col = word_boundary_fwd(cur_line, app->cursor_col);
        break;

    /* Shift+Home — select to line start */
    case KEY_SEL_LSTART:
        if (!app->sel_active) sel_start(app);
        app->cursor_col = 0;
        break;
    /* Shift+End — select to line end */
    case KEY_SEL_LEND:
        if (!app->sel_active) sel_start(app);
        app->cursor_col = cur_len;
        break;

    /* Home / End / Cmd+Left / Cmd+Right */
    case KEY_HOME:
    case KEY_LINE_START:
        sel_clear(app);
        {
            /* Smart Home: first press goes to first non-whitespace,
               subsequent press from there goes to column 0. */
            int ind = 0;
            while (cur_line[ind] == ' ' || cur_line[ind] == '\t') ind++;
            if (app->cursor_col == ind) app->cursor_col = 0;
            else                        app->cursor_col = ind;
        }
        break;
    case KEY_END:
        sel_clear(app); app->cursor_col = cur_len;
        break;
    case KEY_LINE_END:
        /* Ctrl+E: if editing a note → open password change flow */
        if (app->open_note >= 0) {
            NoteVault *nv = app->vault;
            int has = nv ? nv->notes[app->open_note].has_pw : 0;
            app->note_setpw_step = has ? 0 : 1;
            memset(app->note_setpw_old,  0, sizeof(app->note_setpw_old));
            app->note_setpw_old_len  = 0;
            memset(app->note_setpw_new1, 0, sizeof(app->note_setpw_new1));
            app->note_setpw_new1_len = 0;
            memset(app->note_setpw_new2, 0, sizeof(app->note_setpw_new2));
            app->note_setpw_new2_len = 0;
            app->note_setpw_msg[0]   = '\0';
            app->mode = MODE_NOTE_SET_PW;
        } else {
            sel_clear(app); app->cursor_col = cur_len;
        }
        break;

    /* Page Up / Page Down */
    case KEY_PPAGE:
        sel_clear(app);
        app->cursor_row -= view_h;
        cursor_clamp(app); cursor_scroll(app);
        break;
    case KEY_NPAGE:
        sel_clear(app);
        app->cursor_row += view_h;
        cursor_clamp(app); cursor_scroll(app);
        break;

    /* Shift+PgUp / Shift+PgDn — extend selection by page */
    case KEY_SEL_PGUP:
        if (!app->sel_active) sel_start(app);
        app->cursor_row -= view_h;
        cursor_clamp(app); cursor_scroll(app);
        break;
    case KEY_SEL_PGDN:
        if (!app->sel_active) sel_start(app);
        app->cursor_row += view_h;
        cursor_clamp(app); cursor_scroll(app);
        break;

    /* Enter — split line with auto-indent */
    case '\n':
    case '\r':
        if (app->sel_active) edit_delete_sel(app);
        {
            /* Capture leading whitespace of current line for auto-indent */
            const char *cl = app->content.lines[app->cursor_row];
            int ind = 0;
            while (cl[ind] == ' ' || cl[ind] == '\t') ind++;
            /* If cursor is within the leading whitespace, don't carry indent
               (matches what most editors do — empty new line). */
            if (app->cursor_col < ind) ind = 0;
            char *indent_buf = NULL;
            if (ind > 0) {
                indent_buf = malloc(ind + 1);
                if (indent_buf) {
                    memcpy(indent_buf, cl, ind);
                    indent_buf[ind] = '\0';
                }
            }
            edit_split_line(app);
            if (indent_buf) {
                for (int i = 0; i < ind; i++)
                    edit_insert_char(app, indent_buf[i]);
                free(indent_buf);
            }
        }
        break;

    /* Backspace */
    case KEY_BACKSPACE:
    case 127:
    case 8:
        edit_delete_before(app);
        break;

    /* Delete */
    case KEY_DC:
        edit_delete_after(app);
        break;

    /* Tab → 4 spaces */
    case '\t':
        if (app->sel_active) { edit_delete_sel(app); sel_clear(app); }
        for (int i = 0; i < 4; i++) edit_insert_char(app, ' ');
        break;

    case KEY_MOUSE: {
        MEVENT ev;
        if (getmouse(&ev) == OK) {
            if (ev.bstate & BUTTON4_PRESSED) {
                sel_clear(app); cursor_move_up(app);
            } else if (BUTTON_PRESS(ev.bstate, 5)) {
                sel_clear(app); cursor_move_down(app);
            }
        } else {
            sel_clear(app); cursor_move_down(app);
        }
        break;
    }

    default:
        if (key >= 32 && key <= 255) {
            if (app->sel_active) { edit_delete_sel(app); sel_clear(app); }
            unsigned char lead = (unsigned char)key;
            int nb = utf8_char_bytes(lead);
            if (nb == 1) {
                edit_insert_char(app, (char)key);
            } else {
                /* Collect continuation bytes for multi-byte UTF-8 character */
                char utf8buf[5]; utf8buf[0] = (char)lead;
                int got = 1;
                while (got < nb) {
                    int cb = getch();
                    /* continuation bytes are 0x80..0xBF */
                    if (cb < 0x80 || cb > 0xBF) break;
                    utf8buf[got++] = (char)(unsigned char)cb;
                }
                edit_insert_utf8(app, utf8buf, got);
            }
        }
        break;
    }
}

static void handle_edit_find(App *app, int key) {
    switch (key) {
    case 27:   /* Esc → return to edit mode */
        app->mode = MODE_EDIT;
        break;

    case '\n': case '\r':
        find_jump_next(app);
        break;

    case KEY_BACKSPACE:
    case 127:
    case 8:
        if (app->find_query_len > 0) {
            app->find_query[--app->find_query_len] = '\0';
            app->find_match_count = find_total_matches(app);
        } else {
            /* empty + backspace → close find */
            app->mode = MODE_EDIT;
        }
        break;

    /* Ctrl+G — find next */
    case 'g' & 0x1F:
        find_jump_next(app);
        break;

    /* Up arrow — previous match */
    case KEY_UP:
        find_jump_prev(app);
        break;

    /* Down arrow — next match */
    case KEY_DOWN:
        find_jump_next(app);
        break;

    /* Ctrl+U — clear query */
    case 'u' & 0x1F:
        app->find_query[0] = '\0';
        app->find_query_len = 0;
        app->find_match_count = 0;
        break;

    default:
        if (key >= 32 && key <= 126 &&
            app->find_query_len < (int)sizeof(app->find_query) - 1) {
            app->find_query[app->find_query_len++] = (char)key;
            app->find_query[app->find_query_len] = '\0';
            app->find_match_count = find_total_matches(app);
            find_jump_first_from_cursor(app);
        }
        break;
    }
}

static void handle_search(App *app, int key) {
    switch (key) {
    case 27:
    case '\n':
    case '\r':
        app->mode = MODE_LIST;
        curs_set(0);
        break;

    case KEY_BACKSPACE:
    case 127:
    case 8:
        if (app->query_len > 0) {
            app->query[--app->query_len] = '\0';
            update_search(app);
        }
        break;

    case 'u' & 0x1F:  /* Ctrl+U */
        app->query[0]  = '\0';
        app->query_len = 0;
        update_search(app);
        break;

    case KEY_UP:
        app->mode = MODE_LIST;
        curs_set(0);
        handle_list(app, KEY_UP);
        break;

    case KEY_DOWN:
        app->mode = MODE_LIST;
        curs_set(0);
        handle_list(app, KEY_DOWN);
        break;

    case KEY_RIGHT:
    case KEY_ENTER:
        app->mode = MODE_LIST;
        curs_set(0);
        handle_list(app, '\n');
        break;

    default:
        if (key >= 32 && key < 127 &&
            app->query_len < (int)sizeof(app->query) - 2) {
            app->query[app->query_len++] = (char)key;
            app->query[app->query_len]   = '\0';
            /* First search char: upgrade to full scan if still shallow */
            if (!app->scan_full) {
                app->scan_full = 1;
                start_scan(app, -1);
            }
            update_search(app);
        }
        break;
    }
}

static void handle_grep(App *app, int key) {
    int view_h = PANEL_H - 3;

    switch (key) {
    case KEY_UP:
    case 'k':
        if (app->grep_sel > 0) {
            app->grep_sel--;
            if (app->grep_sel < app->grep_offset)
                app->grep_offset = app->grep_sel;
        }
        break;

    case KEY_DOWN:
    case 'j':
        if (app->grep_sel < app->grep_hit_count - 1) {
            app->grep_sel++;
            if (app->grep_sel >= app->grep_offset + view_h)
                app->grep_offset = app->grep_sel - view_h + 1;
        }
        break;

    case KEY_PPAGE:
    case 'u':
        app->grep_sel -= view_h;
        if (app->grep_sel < 0) app->grep_sel = 0;
        app->grep_offset = app->grep_sel;
        break;

    case KEY_NPAGE:
    case 'd':
        app->grep_sel += view_h;
        if (app->grep_sel >= app->grep_hit_count)
            app->grep_sel = app->grep_hit_count - 1;
        if (app->grep_sel < 0) app->grep_sel = 0;
        if (app->grep_sel >= app->grep_offset + view_h)
            app->grep_offset = app->grep_sel - view_h + 1;
        break;

    case 'g': case KEY_HOME:
        app->grep_sel    = 0;
        app->grep_offset = 0;
        break;

    case 'G': case KEY_END:
        app->grep_sel    = app->grep_hit_count - 1;
        app->grep_offset = app->grep_sel - view_h + 1;
        if (app->grep_offset < 0) app->grep_offset = 0;
        break;

    case KEY_RIGHT:
    case '\n':
    case '\r':
        if (app->grep_hit_count > 0) {
            const GrepHit *hit = &app->grep_hits[app->grep_sel];
            load_content(app, hit->file_idx);
            strncpy(app->highlight_word, app->grep_query, sizeof(app->highlight_word) - 1);
            app->highlight_word[sizeof(app->highlight_word) - 1] = '\0';
            int target = hit->line_no - 1;
            app->content_offset = target > 3 ? target - 3 : 0;
            app->mode = MODE_CONTENT;
        }
        break;

    case KEY_LEFT:
    case 27:
        app->mode = MODE_LIST;
        break;

    case 15: /* Ctrl+O — command panel */
        toggle_term(app);
        break;

    case 'q':
    case 'Q':
        app->quit = 1;
        break;
    }
}

/* ---------------------------------------------------------------
   PTY terminal panel (MODE_TERM / Ctrl+O)
--------------------------------------------------------------- */
static void draw_term_panel(const App *app) {
    int sep_y  = PANEL_Y + rpanel_h(app);  /* row of horizontal separator */
    int term_y = sep_y + 1;                /* first row of terminal content */

    /* Separator with label */
    attron(COLOR_PAIR(CP_DIVIDER));
    mvhline(sep_y, RIGHT_X, ACS_HLINE, RIGHT_W);
    attroff(COLOR_PAIR(CP_DIVIDER));
    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvprintw(sep_y, RIGHT_X + 2, " Terminal  [Ctrl+O = toggle] ");
    attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

    if (!app->term) {
        mvprintw(term_y, RIGHT_X + 2, "[terminal unavailable]");
        return;
    }
    if (!app->term->alive) {
        mvprintw(term_y, RIGHT_X + 2, "[ shell exited — press Ctrl+O to close ]");
        return;
    }

    term_draw(app->term, term_y, RIGHT_X, TERM_PANEL_H, RIGHT_W);
}

static void toggle_term(App *app) {
    if (app->term_panel_open) {
        app->term_panel_open = 0;
        app->mode = app->term_prev_mode;
        if (app->mode == MODE_EDIT) curs_set(1); else curs_set(0);
    } else {
        app->term_panel_open = 1;
        app->term_prev_mode  = app->mode;
        app->mode            = MODE_TERM;
        curs_set(1);

        if (!app->term) {
            app->term = term_new();
            if (app->term) {
                int t_cols = RIGHT_W > 10 ? RIGHT_W : 10;
                if (term_open(app->term, TERM_PANEL_H, t_cols) < 0) {
                    term_free(app->term);
                    app->term = NULL;
                }
            }
        }
    }
}

static void handle_terminal(App *app, int key) {
    if (key == 15) { /* Ctrl+O: toggle panel */
        toggle_term(app);
        return;
    }
    if (app->term && app->term->alive)
        term_send_key(app->term, key);
}

/* ---------------------------------------------------------------
   Help overlay (MODE_HELP)
--------------------------------------------------------------- */
static const char *help_lines[] = {
    "  cref — keyboard shortcuts",
    "",
    "  Navigation:",
    "  ↑ ↓            move up/down in list",
    "  → / Enter      open file / expand dir",
    "  ←              collapse dir / go back",
    "  /              search by name/tag/title",
    "  [ ]            jump between sections (content view)",
    "  g / G          top / bottom of file",
    "  q / Q          quit",
    "",
    "  In-app controls:",
    "  Ctrl+Y         toggle notes panel",
    "  Ctrl+N         new file / new note (in notes panel)",
    "  Ctrl+D         delete selected file / note",
    "  Ctrl+T         filter by file type (e.g. c,h)",
    "  Ctrl+L         toggle CWD ↔ C library",
    "  Ctrl+B         build (make if Makefile, else gcc on open .c)",
    "  Ctrl+O         toggle embedded terminal panel",
    "  e              enter editor",
    "",
    "  Notes panel (Ctrl+Y to open):",
    "  ↑ ↓ / j k     navigate notes",
    "  → / Enter      open note",
    "  ←              back to file tree",
    "  e              edit open note",
    "  Ctrl+N         new note (sifre isteği sorulur)",
    "  Ctrl+K         new category/folder",
    "  Ctrl+D         delete note",
    "  /              search in unencrypted notes",
    "",
    "  Not duzenleme (e ile girilir):",
    "  Esc            cik",
    "  Ctrl+S         kaydet",
    "  Ctrl+E         sifre ekle / degistir / kaldir",
    "",
    "  Editor shortcuts (press e to enter):",
    "  Esc            exit editor",
    "  Ctrl+S         save",
    "  Ctrl+Z         undo",
    "  Ctrl+A         select all",
    "  Ctrl+C / X / V copy / cut / paste",
    "  Ctrl+L         select current line",
    "  Ctrl+D         duplicate current line",
    "  Ctrl+K         delete current line",
    "  Ctrl+F / G     find / find next",
    "  Shift+←→↑↓     extend selection",
    "  Ctrl+←→        word jump (works over SSH)",
    "  Ctrl+Shift+←→  word-by-word selection (works over SSH)",
    "  Fn+←→          line start / end (Mac)",
    "  Shift+Fn+←→    select to line start / end",
    "  F1             this help screen",
    "",
    "  Build output (Ctrl+B):",
    "  ↑ ↓ / j k / PgUp PgDn   scroll",
    "  g / G          top / bottom",
    "  Ctrl+B         rebuild",
    "  Esc            close",
    "",
    "  Press any key to close this help.",
    NULL
};

static void draw_help(const App *app) {
    clear();
    int clip = COLS - 4;
    int rows = LINES - 1;
    int offset = app->help_offset;

    /* count total lines */
    int total = 0;
    while (help_lines[total]) total++;

    for (int row = 0; row < rows; row++) {
        int li = offset + row;
        if (li >= total) break;
        if (li == 0) {
            attron(COLOR_PAIR(CP_MD_HEAD) | A_BOLD);
            print_clipped(row, 2, clip, help_lines[0]);
            attroff(COLOR_PAIR(CP_MD_HEAD) | A_BOLD);
        } else {
            print_clipped(row, 2, clip, help_lines[li]);
        }
    }

    /* scroll indicator at bottom right */
    if (total > rows) {
        char ind[16];
        snprintf(ind, sizeof(ind), "%d/%d", offset + 1, total - rows + 1);
        mvprintw(LINES - 1, COLS - (int)strlen(ind) - 2, "%s", ind);
    }
}

static void handle_help(App *app, int key) {
    int total = 0;
    while (help_lines[total]) total++;
    int max_off = total - (LINES - 1);
    if (max_off < 0) max_off = 0;

    switch (key) {
    case KEY_UP:   case 'k':
        if (app->help_offset > 0) app->help_offset--;
        return;
    case KEY_DOWN: case 'j':
        if (app->help_offset < max_off) app->help_offset++;
        return;
    case KEY_PPAGE:
        app->help_offset -= LINES - 2;
        if (app->help_offset < 0) app->help_offset = 0;
        return;
    case KEY_NPAGE:
        app->help_offset += LINES - 2;
        if (app->help_offset > max_off) app->help_offset = max_off;
        return;
    case 'g':
        app->help_offset = 0;
        return;
    case 'G':
        app->help_offset = max_off;
        return;
    case KEY_MOUSE: {
        MEVENT ev;
        if (getmouse(&ev) == OK) {
            if (ev.bstate & BUTTON4_PRESSED) {
                if (app->help_offset > 0) app->help_offset--;
            } else if (BUTTON_PRESS(ev.bstate, 5)) {
                if (app->help_offset < max_off) app->help_offset++;
            }
        } else {
            if (app->help_offset < max_off) app->help_offset++;
        }
        return;
    }
    default:
        break;
    }
    /* any other key closes help */
    app->mode = app->help_prev_mode;
    if (app->mode == MODE_EDIT)
        curs_set(1);
}

/* ---------------------------------------------------------------
   Main loop
--------------------------------------------------------------- */
void tui_run(App *app) {
    pthread_mutex_init(&app->scan_mutex, NULL);

    /* Start background scan immediately; UI opens with empty file list.
       Library is always full; CWD starts with a shallow 2-level scan. */
    if (app->in_lib_view) {
        app->scan_full = 1;
        start_scan(app, -1);
    } else {
        app->scan_full = 0;
        start_scan(app, 2);
    }

    if (app->mode == MODE_GREP) {
        /* Grep needs all files — if we started a shallow scan, restart full */
        if (!app->scan_full) {
            app->scan_full = 1;
            start_scan(app, -1);
        }
        if (app->scan_running) {
            pthread_join(app->scan_thread, NULL);
            app->scan_running = 0;
            apply_scan_result(app);
        }
        grep_collect(app);
    }

    update_search(app);
    build_tree(app);

    /* Optional key-press log for diagnosing terminal escape sequences.
       Enable with: CREF_KEYLOG=/tmp/keys.log cref ... */
    FILE *klog = NULL;
    const char *klog_path = getenv("CREF_KEYLOG");
    if (klog_path && *klog_path) klog = fopen(klog_path, "w");

    while (!app->quit) {
        /* Poll at 50 ms while scanning or PTY is alive */
        int need_poll = app->scan_running || (app->term && app->term->alive);
        timeout(need_poll ? 50 : -1);

        redraw(app);
        int key = getch();

        /* Drain any pending PTY output before processing keys */
        if (app->term)
            term_pump(app->term);

        /* Pick up completed scan results */
        check_scan_done(app);

        if (app->scan_running) app->scan_frame++;

        if (key == ERR) continue;  /* timeout, no key pressed */

        if (klog) {
            fprintf(klog, "%d 0x%X\n", key, key);
            fflush(klog);
        }

        if (key == KEY_RESIZE) {
            if (app->term && app->term->alive) {
                int new_cols = RIGHT_W > 10 ? RIGHT_W : 10;
                term_resize(app->term, TERM_PANEL_H, new_cols);
            }
            continue;
        }

        /* Drain mouse events for modes that don't handle scroll.
           Handlers that DO use getmouse() (list/content/edit) call it themselves. */
        if (key == KEY_MOUSE) {
            int handles_scroll = (app->mode == MODE_LIST ||
                                  app->mode == MODE_CONTENT ||
                                  app->mode == MODE_EDIT ||
                                  app->mode == MODE_NOTES);
            if (!handles_scroll) {
                MEVENT ev;
                getmouse(&ev);
                continue;
            }
        }

        switch (app->mode) {
        case MODE_LIST:         handle_list(app, key);         break;
        case MODE_CONTENT:      handle_content(app, key);      break;
        case MODE_SEARCH:       handle_search(app, key);       break;
        case MODE_GREP:         handle_grep(app, key);         break;
        case MODE_EDIT:         handle_edit(app, key);         break;
        case MODE_EDIT_FIND:    handle_edit_find(app, key);    break;
        case MODE_FILTER:       handle_filter(app, key);       break;
        case MODE_GOTO:         handle_goto(app, key);         break;
        case MODE_NEW_FILE:     handle_new_file(app, key);     break;
        case MODE_CONFIRM_DEL:  handle_confirm_del(app, key);  break;
        case MODE_COMPILE_OUT:  handle_compile_out(app, key);  break;
        case MODE_HELP:         handle_help(app, key);         break;
        case MODE_TERM:         handle_terminal(app, key);     break;
        case MODE_NOTES:        handle_notes(app, key);        break;
        case MODE_NOTE_PW:      handle_note_pw(app, key);      break;
        case MODE_NOTE_NEW:     handle_note_new(app, key);     break;
        case MODE_NOTE_NEW_CAT: handle_note_new_cat(app, key); break;
        case MODE_NOTE_SEARCH:  handle_note_search(app, key);  break;
        case MODE_NOTE_SET_PW:  handle_note_set_pw(app, key);  break;
        }
    }

    if (klog) fclose(klog);

    /* Cancel + join any still-running scan thread before cleanup */
    if (app->scan_running) {
        app->scan_cancel = 1;
        pthread_join(app->scan_thread, NULL);
        app->scan_running = 0;
    }
    free(app->scan_pending);
    app->scan_pending = NULL;
    free(app->scan_pending_disc);
    app->scan_pending_disc = NULL;
    pthread_mutex_destroy(&app->scan_mutex);

    boundary_dirs_free(app);
    loaded_dirs_free(app);

    if (app->vault) {
        sodium_memzero(app->note_active_pw, sizeof(app->note_active_pw));
        vault_close(app->vault);
        free(app->vault);
        app->vault = NULL;
    }

    if (app->term) {
        term_free(app->term);
        app->term = NULL;
    }
}
