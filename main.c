#include "meta.h"
#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

static int dir_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/* Find the reference directory:
   1. CREF_DIR environment variable
   2. ~/.local/share/cref/ref
   3. /usr/local/share/cref/ref
   4. ./c-rehberi  (development fallback) */
static const char *find_dir(char *buf, int len) {
    const char *env = getenv("CREF_DIR");
    if (env && dir_exists(env)) return env;

    const char *home = getenv("HOME");
    if (home) {
        snprintf(buf, len, "%s/.local/share/cref/ref", home);
        if (dir_exists(buf)) return buf;
    }

    if (dir_exists("/usr/local/share/cref/ref"))
        return "/usr/local/share/cref/ref";

    return "./ref";
}

/* Parse "md,txt,php" into exts[]/ext_count.
   buf is a writable scratch buffer (ext pointers point into it). */
static int parse_filetypes(char *buf, const char **exts, int max_exts) {
    int count = 0;
    char *save = NULL;
    char *tok = strtok_r(buf, ",", &save);
    while (tok && count < max_exts) {
        while (*tok == ' ' || *tok == '.') tok++;   /* strip leading dot/space */
        char *end = tok + strlen(tok) - 1;
        while (end > tok && (*end == ' ' || *end == '.')) *end-- = '\0';
        if (*tok) exts[count++] = tok;
        tok = strtok_r(NULL, ",", &save);
    }
    return count;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options] [query|directory]\n\n"
        "Options:\n"
        "  -d <dir>              Directory to browse (default: current directory)\n"
        "  --ref                 Open the C stdlib reference library instead of CWD\n"
        "  --filetype <exts>     Comma-separated extensions to scan (default: all)\n"
        "                        Example: --filetype c,h,md\n"
        "  --grep <word>         Search inside file contents (TUI, scrollable)\n"
        "  --score <word>        Count matches per file, print and exit\n"
        "  -h, --help            Show this help\n\n"
        "Positional argument (no flag):\n"
        "  existing path    → use as directory\n"
        "  any other text   → pre-fill TUI search query\n\n"
        "In-app shortcuts:\n"
        "  Ctrl+N    → create a new file\n"
        "  Ctrl+D    → delete selected file (with confirmation)\n"
        "  Ctrl+T    → filter by file type (e.g. c,h)\n"
        "  Ctrl+L    → toggle between CWD and library\n"
        "  Ctrl+B    → build (make if Makefile exists, else gcc on open .c file)\n\n"
        "Examples:\n"
        "  %s\n"
        "  %s --ref\n"
        "  %s malloc\n"
        "  %s --grep malloc\n"
        "  %s --score pointer\n"
        "  %s -d ~/my-project --filetype c,h\n",
        prog, prog, prog, prog, prog, prog, prog);
}

/* --score: count literal occurrences of query in each file, print sorted. */
static int run_score(const char *dir, const char *query,
                     const char * const *exts, int ext_count) {
    int n = 0;
    FileMeta *files = scan_dir(dir, &n, exts, ext_count, -1, NULL, NULL, NULL, NULL, NULL);
    if (!files) {
        fprintf(stderr, "Error: cannot open directory '%s'\n", dir);
        return 1;
    }
    if (n == 0) {
        fprintf(stderr, "No matching files found in '%s'\n", dir);
        free(files);
        return 1;
    }

    typedef struct { int idx; int count; } SC;
    SC *scores = malloc(n * sizeof(SC));
    if (!scores) { free(files); fprintf(stderr, "Out of memory\n"); return 1; }
    int sc_n  = 0;
    int total = 0;
    int qlen  = (int)strlen(query);

    for (int fi = 0; fi < n; fi++) {
        FILE *f = fopen(files[fi].path, "r");
        if (!f) continue;

        int   count = 0;
        char  line[1024];

        while (fgets(line, sizeof(line), f)) {
            const char *p = line;
            while (*p) {
                int ok = 1;
                for (int j = 0; j < qlen && p[j]; j++) {
                    if (tolower((unsigned char)p[j]) !=
                        tolower((unsigned char)query[j])) { ok = 0; break; }
                }
                if (ok && qlen > 0) { count++; p += qlen; }
                else p++;
            }
        }
        fclose(f);

        if (count > 0) {
            scores[sc_n].idx   = fi;
            scores[sc_n].count = count;
            sc_n++;
            total += count;
        }
    }

    /* Sort descending by count (insertion sort) */
    for (int i = 1; i < sc_n; i++) {
        SC tmp = scores[i];
        int j  = i - 1;
        while (j >= 0 && scores[j].count < tmp.count) {
            scores[j + 1] = scores[j];
            j--;
        }
        scores[j + 1] = tmp;
    }

    printf("\nScore report: \"%s\"\n", query);
    printf("────────────────────────────────────────────────\n");
    for (int i = 0; i < sc_n; i++)
        printf("  %4d  %s\n", scores[i].count, files[scores[i].idx].filename);
    printf("────────────────────────────────────────────────\n");
    printf("  %4d  matches in %d file%s\n\n",
           total, sc_n, sc_n == 1 ? "" : "s");
    free(scores);
    free(files);
    return 0;
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    char        cwd_buf[META_PATH_LEN] = "";
    char        lib_dir_buf[META_PATH_LEN] = "";
    const char *dir         = NULL;
    const char *grep_query  = NULL;
    const char *score_query = NULL;
    const char *tui_query   = NULL;
    int         use_ref     = 0;

    /* Extension list — default is all files (ext_count = 0) */
    char        ext_buf[256] = "";
    const char *exts[16]     = { NULL };
    int         ext_count    = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) { usage(argv[0]); return 1; }
            dir = argv[++i];
        } else if (strcmp(argv[i], "--ref") == 0) {
            use_ref = 1;
        } else if (strcmp(argv[i], "--filetype") == 0) {
            if (i + 1 >= argc) { usage(argv[0]); return 1; }
            strncpy(ext_buf, argv[++i], sizeof(ext_buf) - 1);
            ext_count = parse_filetypes(ext_buf, exts, 16);
        } else if (strcmp(argv[i], "--grep") == 0) {
            if (i + 1 >= argc) { usage(argv[0]); return 1; }
            grep_query = argv[++i];
        } else if (strcmp(argv[i], "--score") == 0) {
            if (i + 1 >= argc) { usage(argv[0]); return 1; }
            score_query = argv[++i];
        } else if (argv[i][0] != '-') {
            if (dir_exists(argv[i]))
                dir = argv[i];
            else
                tui_query = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    /* Always resolve library dir (needed for Ctrl+L toggle even in CWD mode).
       find_dir may return env, buf, or a static string — capture the result. */
    {
        const char *found = find_dir(lib_dir_buf, sizeof(lib_dir_buf));
        if (found != lib_dir_buf)
            strncpy(lib_dir_buf, found, sizeof(lib_dir_buf) - 1);
    }

    if (!dir) {
        if (use_ref) {
            dir = lib_dir_buf;
            if (ext_count == 0) {
                strncpy(ext_buf, "md", sizeof(ext_buf) - 1);
                ext_count = parse_filetypes(ext_buf, exts, 16);
            }
        } else {
            if (!getcwd(cwd_buf, sizeof(cwd_buf)))
                strncpy(cwd_buf, ".", sizeof(cwd_buf) - 1);
            dir = cwd_buf;
        }
    }

    if (score_query)
        return run_score(dir, score_query, exts, ext_count);

    App *app = calloc(1, sizeof(App));
    if (!app) { fprintf(stderr, "Out of memory\n"); return 1; }
    app->open_file = -1;
    app->open_note = -1;
    app->mode      = MODE_LIST;

    strncpy(app->scan_dir_path, dir,        META_PATH_LEN - 1);
    strncpy(app->lib_dir,       lib_dir_buf, META_PATH_LEN - 1);
    app->in_lib_view = use_ref ? 1 : 0;

    /* Store initial extension filter in app (filter_exts point into filter_buf) */
    if (ext_count > 0) {
        strncpy(app->filter_buf, ext_buf, sizeof(app->filter_buf) - 1);
        app->filter_ext_count = parse_filetypes(app->filter_buf, app->filter_exts, 16);
    }
    /* filter_ext_count == 0 means show all file types */

    /* Pre-validate directory before entering TUI */
    {
        DIR *test = opendir(dir);
        if (!test) {
            fprintf(stderr, "Error: cannot open directory '%s'\n", dir);
            fprintf(stderr, "Tip: use -d <dir>, or pass a directory path.\n");
            free(app);
            return 1;
        }
        closedir(test);
    }
    /* Files are loaded by the background scan thread started in tui_run */

    if (grep_query) {
        strncpy(app->grep_query, grep_query, sizeof(app->grep_query) - 1);
        app->mode = MODE_GREP;
    } else if (tui_query) {
        strncpy(app->query, tui_query, sizeof(app->query) - 1);
        app->query_len = (int)strlen(app->query);
    }

    tui_init();
    tui_run(app);
    tui_cleanup();

    content_free(&app->content);
    content_free(&app->compile_out);
    clipboard_free(app);
    bat_free(app);
    free(app->files);
    free(app->filtered);
    free(app);
    return 0;
}
