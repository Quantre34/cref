#ifndef TUI_H
#define TUI_H

#include "meta.h"
#include "notes.h"
#include "term.h"
#include <ncurses.h>
#include <pthread.h>

#define GREP_MAX_HITS  1000
#define TREE_MAX_NODES 10000
#define UNDO_CAP       200

typedef struct {
    int  file_idx;
    int  line_no;
    char line[256];
    int  match_col;
    int  match_len;
} GrepHit;

typedef enum {
    MODE_LIST,
    MODE_CONTENT,
    MODE_SEARCH,
    MODE_GREP,
    MODE_EDIT,
    MODE_EDIT_FIND,   /* in-file find prompt over the editor */
    MODE_FILTER,      /* Ctrl+T: file-type extension filter prompt */
    MODE_GOTO,        /* Ctrl+G: go to directory prompt */
    MODE_NEW_FILE,    /* Ctrl+N: new file name prompt */
    MODE_NEW_DIR,     /* Ctrl+K: new directory name prompt */
    MODE_CONFIRM_DEL, /* Ctrl+D: delete confirmation prompt */
    MODE_COMPILE_OUT, /* Ctrl+B: build output viewer */
    MODE_HELP,        /* ?: shortcuts overlay */
    MODE_TERM,        /* Ctrl+O: embedded PTY terminal panel */
    MODE_NOTES,       /* Ctrl+Y: notes tree panel active */
    MODE_NOTE_PW,     /* password prompt for locked note */
    MODE_NOTE_NEW,     /* new note title prompt */
    MODE_NOTE_NEW_CAT, /* Ctrl+K: new category/folder prompt */
    MODE_NOTE_SEARCH,  /* search inside notes */
    MODE_NOTE_SET_PW,  /* Ctrl+E: set/change/remove note password */
} Mode;

typedef enum { NVIEW_CAT = 0, NVIEW_NOTE = 1 } NViewKind;
typedef struct { NViewKind kind; int idx; } NViewItem;

typedef struct { int col; int len; short cp; attr_t attr; } HLSpan;

typedef struct {
    char **lines;
    int    line_count;
    int    capacity;
} Content;

typedef enum { NODE_DIR, NODE_FILE } NodeKind;

typedef struct {
    NodeKind kind;
    int      depth;
    int      expanded;
    int      subtree_size;
    char     name[64];
    char     subdir[META_SUBDIR_LEN];
    int      file_idx;
    int      total_files;
} TreeNode;

typedef enum {
    UNDO_CHAR_INS,   /* single char inserted → undo = delete it */
    UNDO_CHAR_DEL,   /* single char deleted  → undo = re-insert */
    UNDO_SPLIT,      /* Enter: split at (row,col) → undo = join row+row+1 */
    UNDO_JOIN,       /* Backspace at col0: joined row-1 with row → undo = re-split */
    UNDO_BLOCK_DEL,  /* selection deleted → undo = re-insert text */
    UNDO_BLOCK_INS,  /* paste inserted    → undo = delete range */
    UNDO_LINE_DUP,   /* line duplicated at row → undo = delete line row+1 */
    UNDO_LINE_DEL,   /* line at row deleted → undo = re-insert text at row */
    UNDO_LINE_SWAP,  /* rows row ↔ row2 swapped → undo = swap them back */
} UndoKind;

typedef struct {
    UndoKind kind;
    int  row, col;   /* operation anchor */
    int  row2, col2; /* UNDO_BLOCK_*: other end of range */
    char *text;      /* UNDO_CHAR_DEL: 1-char str; UNDO_BLOCK_DEL: \n-joined block */
} UndoEntry;

typedef struct {
    /* File database — heap allocated, may be NULL while scanning */
    FileMeta *files;
    int       file_count;

    /* Search results (indices into files[]) — heap allocated */
    int *filtered;
    int  filtered_count;

    /* Search box */
    char query[128];
    int  query_len;
    char suggestion[64];

    /* Filtered list panel state */
    int list_sel;
    int list_offset;

    /* Content panel state */
    Content content;
    int     content_offset;
    int     open_file;

    /* Highlight word when file opened from search/grep */
    char highlight_word[64];

    /* Grep mode state */
    GrepHit grep_hits[GREP_MAX_HITS];
    int     grep_hit_count;
    int     grep_sel;
    int     grep_offset;
    char    grep_query[128];

    /* Tree state (used when query is empty) */
    TreeNode tree_all[TREE_MAX_NODES];
    int      tree_all_count;
    int      tree_view[TREE_MAX_NODES];
    int      tree_view_count;
    int      tree_sel;
    int      tree_offset;

    /* Editor cursor (MODE_EDIT) */
    int cursor_row;
    int cursor_col;

    /* Selection */
    int sel_active;
    int sel_anchor_row;
    int sel_anchor_col;

    /* Clipboard */
    char **clipboard;
    int    clipboard_count;

    /* Undo stack */
    UndoEntry undo_stack[UNDO_CAP];
    int       undo_top;
    int       undo_count;

    /* Unsaved changes flag */
    int content_dirty;
    int save_flash;    /* > 0: show "Saved!" in status bar, decrements each keypress */

    /* In-file find (MODE_EDIT_FIND) */
    char find_query[128];
    int  find_query_len;
    int  find_match_count;   /* total matches in file (recomputed on query change) */

    /* Diagnostic: last received key code (shown in MODE_EDIT status bar) */
    int last_key;

    /* Directory state */
    char scan_dir_path[META_PATH_LEN]; /* current directory being shown */
    char lib_dir[META_PATH_LEN];       /* C reference library directory */
    int  in_lib_view;                  /* 1 = showing lib, 0 = CWD */

    /* Filetype filter (Ctrl+T) */
    char filter_buf[256];              /* parsed ext storage (modified by strtok) */
    char filter_input[256];            /* raw input during MODE_FILTER */
    int  filter_input_len;
    const char *filter_exts[16];       /* pointers into filter_buf */
    int  filter_ext_count;             /* 0 = show all file types */

    /* Go-to directory (Ctrl+G) */
    char goto_buf[META_PATH_LEN];
    int  goto_len;

    /* New-file prompt (MODE_NEW_FILE) */
    char new_file_buf[META_NAME_LEN];
    int  new_file_len;
    char new_file_msg[128];

    /* New-dir prompt (MODE_NEW_DIR) */
    char new_dir_buf[META_NAME_LEN];
    int  new_dir_len;
    char new_dir_msg[128];

    /* Delete confirmation (MODE_CONFIRM_DEL) */
    int  del_pending_idx;              /* files[] index at confirm time (-1 = none) */
    char del_pending_path[META_PATH_LEN]; /* actual path — stable across rescans */
    int  del_prev_mode;               /* mode to return to on cancel */

    /* bat syntax spans (NULL if bat not used / not available) */
    HLSpan **bat_rows;
    int     *bat_row_sizes;
    int      bat_nrows;

    /* Compile output (MODE_COMPILE_OUT) */
    Content compile_out;
    int     compile_out_off;
    char    compile_status[256];
    int     compile_ok;
    int     compile_prev_mode;

    /* Help overlay (MODE_HELP) */
    int     help_prev_mode;
    int     help_offset;

    /* PTY terminal panel (MODE_TERM / Ctrl+O) */
    Term   *term;
    int     term_panel_open;
    int     term_prev_mode;

    /* Background scan thread */
    pthread_t        scan_thread;
    pthread_mutex_t  scan_mutex;
    volatile int     scan_running;       /* 1 = thread is active */
    volatile int     scan_done;          /* 1 = final result ready, join thread */
    volatile int     scan_batch_ready;   /* 1 = partial batch in scan_pending */
    volatile int     scan_cancel;        /* set to 1 to abort running scan */
    volatile int     scan_live_count;    /* file count updated every SCAN_BATCH_SIZE */
    int              scan_batch_threshold; /* next count at which to take a full snapshot */
    int              scan_full;          /* 1 = full (unlimited depth) scan done/running */
    int              scan_frame;         /* spinner tick counter */
    FileMeta        *scan_pending;       /* result written by thread */
    int              scan_pending_count; /* result count written by thread */
    char           (*scan_pending_disc)[META_SUBDIR_LEN]; /* boundary dirs from thread */
    int              scan_pending_disc_count;

    /* Boundary dirs: discovered but not yet recursed into */
    char           (*boundary_dirs)[META_SUBDIR_LEN];
    int              boundary_dir_count;

    /* Subdirs that have been fully loaded into files[] */
    char           (*loaded_dirs)[META_SUBDIR_LEN];
    int              loaded_dir_count;
    int              loaded_dir_cap;

    /* Pending on-demand expansion queue (processed one at a time) */
    char             expand_queue[8][META_SUBDIR_LEN];
    int              expand_queue_count;

    /* Currently loading subdir (empty string if none) */
    char             loading_subdir[META_SUBDIR_LEN];

    /* Notes panel (Ctrl+Y) */
    NoteVault *vault;                   /* heap, NULL until first Ctrl+Y */
    int        open_note;               /* -1 = no note open */
    char       note_active_pw[256];     /* pw for currently open note */

    NViewItem  notes_view[NOTE_MAX_NOTES + NOTE_MAX_CATS + 4];
    int        notes_view_count;
    int        notes_sel;
    int        notes_offset;

    /* MODE_NOTE_PW */
    int        note_pw_idx;             /* note waiting for password */
    char       note_pw_buf[256];
    int        note_pw_len;
    int        note_pw_visible;         /* 1 = show chars (creation) */

    /* MODE_NOTE_NEW */
    char       note_new_title[NOTE_TITLE_LEN];
    int        note_new_title_len;
    int        note_new_cat_sel;        /* category index */
    int        note_new_step;           /* 0=title, 1=ask pw y/n, 2=pw, 3=confirm */
    int        note_new_created_idx;    /* vault idx of freshly created note */
    char       note_new_pw1[256];
    int        note_new_pw1_len;
    char       note_new_pw2[256];
    int        note_new_pw2_len;

    /* MODE_NOTE_NEW_CAT (Ctrl+K in notes panel) */
    char note_new_cat_buf[NOTE_CAT_LEN];
    int  note_new_cat_len;

    /* MODE_NOTE_SET_PW (Ctrl+E in edit mode) */
    int        note_setpw_step;         /* 0=current pw, 1=new pw, 2=confirm */
    char       note_setpw_old[256];
    int        note_setpw_old_len;
    char       note_setpw_new1[256];
    int        note_setpw_new1_len;
    char       note_setpw_new2[256];
    int        note_setpw_new2_len;
    char       note_setpw_msg[128];

    /* MODE_NOTE_SEARCH */
    char       note_sq[128];
    int        note_sq_len;
    NoteHit    note_hits[512];
    int        note_hit_count;
    int        note_hit_sel;
    int        note_hit_offset;

    Mode mode;
    int  quit;
    int  pasting;   /* 1 while bracketed paste is in progress */
} App;

void tui_init(void);
void tui_cleanup(void);
void tui_run(App *app);
void content_free(Content *c);
void clipboard_free(App *a);
void bat_free(App *a);

#endif
