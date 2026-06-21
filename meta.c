#include "meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static void strip_quotes(char *s) {
    int n = strlen(s);
    if (n >= 2 &&
        ((s[0] == '"' && s[n-1] == '"') ||
         (s[0] == '\'' && s[n-1] == '\''))) {
        memmove(s, s + 1, n - 2);
        s[n - 2] = '\0';
    }
}

static void read_frontmatter(const char *path, FileMeta *meta) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[512];
    int  lineno  = 0;
    int  in_fm   = 0;
    int  past_fm = 0;
    int  slen    = 0;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        lineno++;

        if (!past_fm) {
            if (lineno == 1 && strcmp(line, "---") == 0) {
                in_fm = 1;
                continue;
            }
            if (!in_fm) {
                past_fm = 1;
            } else if (strcmp(line, "---") == 0) {
                in_fm   = 0;
                past_fm = 1;
                continue;
            } else {
                char *colon = strchr(line, ':');
                if (!colon) continue;
                *colon = '\0';
                char *key = trim(line);
                char *val = trim(colon + 1);

                if (strcmp(key, "baslik") == 0) {
                    strip_quotes(val);
                    strncpy(meta->title, val, META_TITLE_LEN - 1);
                } else if (strcmp(key, "kategori") == 0) {
                    strip_quotes(val);
                    strncpy(meta->category, val, META_CAT_LEN - 1);
                } else if (strcmp(key, "zorluk") == 0) {
                    strip_quotes(val);
                    strncpy(meta->difficulty, val, META_DIFF_LEN - 1);
                } else if (strcmp(key, "etiketler") == 0) {
                    char *open = strchr(val, '[');
                    if (!open) continue;
                    open++;
                    char *close = strrchr(open, ']');
                    if (close) *close = '\0';
                    char buf[512];
                    strncpy(buf, open, sizeof(buf) - 1);
                    buf[sizeof(buf) - 1] = '\0';
                    char *tag_save = NULL;
                    char *tok = strtok_r(buf, ",", &tag_save);
                    meta->tag_count = 0;
                    while (tok && meta->tag_count < META_MAX_TAGS) {
                        char *t = trim(tok);
                        strip_quotes(t);
                        if (*t) {
                            strncpy(meta->tags[meta->tag_count], t, META_TAG_LEN - 1);
                            meta->tag_count++;
                        }
                        tok = strtok_r(NULL, ",", &tag_save);
                    }
                }
                continue;
            }
        }

        if (past_fm && line[0] != '\0' &&
            slen < (int)sizeof(meta->snippet) - 2) {
            if (slen > 0) meta->snippet[slen++] = ' ';
            int ll   = (int)strlen(line);
            int room = (int)sizeof(meta->snippet) - slen - 1;
            if (ll > room) ll = room;
            memcpy(meta->snippet + slen, line, ll);
            slen += ll;
            meta->snippet[slen] = '\0';
        }
        if (slen >= 200) break;
    }

    fclose(f);
}

static int count_lines(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int count = 0, c, last = '\n';
    while ((c = fgetc(f)) != EOF) { last = c; if (c == '\n') count++; }
    if (last != '\n') count++;
    fclose(f);
    return count;
}

static const char *SKIP_FILES[] = { "INDEX.md", "FRONTMATTER-FORMAT.md", NULL };
static const char *SKIP_DIRS[]  = {
    "node_modules", ".git", "dist", ".cache",
    "vendor", "target", ".next", "coverage",
    "proc", "sys", "dev", "__pycache__", ".venv", "snap",
    NULL
};

/* Absolute path prefixes to skip (macOS system dirs, Linux pseudo-filesystems) */
static const char *SKIP_ABS[] = {
    "/System/Volumes",
    "/private/var/folders",
    NULL
};

static int should_skip(const char *name) {
    for (int i = 0; SKIP_FILES[i]; i++)
        if (strcmp(name, SKIP_FILES[i]) == 0) return 1;
    return 0;
}

static int should_skip_dir(const char *name) {
    for (int i = 0; SKIP_DIRS[i]; i++)
        if (strcmp(name, SKIP_DIRS[i]) == 0) return 1;
    return 0;
}

static int should_skip_abs(const char *path) {
    for (int i = 0; SKIP_ABS[i]; i++) {
        int len = (int)strlen(SKIP_ABS[i]);
        if (strncmp(path, SKIP_ABS[i], len) == 0 &&
            (path[len] == '\0' || path[len] == '/'))
            return 1;
    }
    return 0;
}

static int is_binary_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char buf[512];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    for (size_t i = 0; i < n; i++)
        if (buf[i] == 0) return 1;
    return 0;
}

static int match_ext(const char *name, const char * const *exts, int ext_count) {
    if (ext_count == 0) return 1;
    int nlen = (int)strlen(name);
    for (int i = 0; i < ext_count; i++) {
        int elen = (int)strlen(exts[i]);
        if (nlen > elen + 1 &&
            name[nlen - elen - 1] == '.' &&
            strcmp(name + nlen - elen, exts[i]) == 0)
            return 1;
    }
    return 0;
}

static int cmp_subdir_filename(const void *a, const void *b) {
    const FileMeta *fa = (const FileMeta *)a;
    const FileMeta *fb = (const FileMeta *)b;
    int c = strcmp(fa->subdir, fb->subdir);
    return c ? c : strcmp(fa->filename, fb->filename);
}

/* Return the BFS depth of a relative path: "" → 0, "a" → 1, "a/b" → 2 */
static int rel_depth(const char *rel) {
    if (!rel || !rel[0]) return 0;
    int d = 1;
    for (const char *p = rel; *p; p++)
        if (*p == '/') d++;
    return d;
}

FileMeta *scan_dir(const char *dir, int *count_out,
                   const char * const *exts, int ext_count,
                   int max_depth,
                   char (**disc_out)[META_SUBDIR_LEN], int *disc_count_out,
                   ScanProgressFn progress_fn, void *progress_ctx,
                   volatile int *cancel) {
    DIR *test = opendir(dir);
    if (!test) { *count_out = 0; return NULL; }
    closedir(test);

    if (!exts) ext_count = 0;

    /* BFS queue of relative subdirectory paths */
    int bfs_cap = 4096, qhead = 0, qtail = 0;
    char (*bfs_q)[META_SUBDIR_LEN] = malloc(bfs_cap * sizeof(*bfs_q));
    if (!bfs_q) { *count_out = 0; return NULL; }
    bfs_q[qtail][0] = '\0';   /* root: relative path = "" */
    qtail++;

    /* Discovered (boundary) dirs: found but not recursed due to max_depth */
    int disc_cap = 64, disc_count = 0;
    char (*disc_arr)[META_SUBDIR_LEN] = NULL;
    if (disc_out && disc_count_out) {
        disc_arr = malloc(disc_cap * sizeof(*disc_arr));
        /* non-fatal if alloc fails — we just won't track boundary dirs */
    }

    int cap = 256, count = 0, last_flush = 0;
    FileMeta *files = calloc(cap, sizeof(FileMeta));
    if (!files) { free(bfs_q); free(disc_arr); *count_out = 0; return NULL; }

    while (qhead < qtail) {
        if (cancel && *cancel) break;
        const char *rel = bfs_q[qhead++];

        char full[META_PATH_LEN];
        if (rel[0])
            snprintf(full, sizeof(full), "%s/%s", dir, rel);
        else
            strncpy(full, dir, sizeof(full) - 1);

        if (should_skip_abs(full)) continue;

        DIR *d = opendir(full);
        if (!d) continue;

        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (cancel && *cancel) break;
            const char *name = entry->d_name;
            if (name[0] == '.') continue;

            char child[META_PATH_LEN];
            snprintf(child, sizeof(child), "%s/%s", full, name);

            struct stat st;
            if (lstat(child, &st) != 0) continue;
            if (S_ISLNK(st.st_mode))    continue;

            if (S_ISDIR(st.st_mode)) {
                if (should_skip_dir(name))  continue;
                if (should_skip_abs(child)) continue;
                /* Depth limit: child depth = depth(rel) + 1 */
                if (max_depth >= 0 && rel_depth(rel) + 1 > max_depth) {
                    /* Record as boundary dir (discovered but not opened) */
                    if (disc_arr) {
                        if (disc_count >= disc_cap) {
                            int nc = disc_cap * 2;
                            char (*tmp)[META_SUBDIR_LEN] = realloc(disc_arr, nc * sizeof(*disc_arr));
                            if (tmp) { disc_arr = tmp; disc_cap = nc; }
                        }
                        if (disc_count < disc_cap) {
                            char rel_child[META_SUBDIR_LEN];
                            if (rel[0])
                                snprintf(rel_child, META_SUBDIR_LEN, "%s/%s", rel, name);
                            else
                                strncpy(rel_child, name, META_SUBDIR_LEN - 1);
                            strncpy(disc_arr[disc_count++], rel_child, META_SUBDIR_LEN - 1);
                        }
                    }
                    continue;
                }
                /* Grow BFS queue if needed */
                if (qtail >= bfs_cap) {
                    int nc = bfs_cap * 2;
                    char (*tmp)[META_SUBDIR_LEN] = realloc(bfs_q, nc * sizeof(*bfs_q));
                    if (!tmp) continue;
                    bfs_q = tmp; bfs_cap = nc;
                }
                if (rel[0])
                    snprintf(bfs_q[qtail], META_SUBDIR_LEN, "%s/%s", rel, name);
                else
                    strncpy(bfs_q[qtail], name, META_SUBDIR_LEN - 1);
                qtail++;

            } else if (S_ISREG(st.st_mode)) {
                if (!match_ext(name, exts, ext_count)) continue;
                if (should_skip(name))                 continue;

                if (count >= cap) {
                    int nc = cap * 2;
                    FileMeta *tmp = realloc(files, nc * sizeof(FileMeta));
                    if (!tmp) goto done;
                    files = tmp; cap = nc;
                }

                FileMeta *m = &files[count];
                memset(m, 0, sizeof(*m));
                strncpy(m->path,     child, META_PATH_LEN - 1);
                strncpy(m->filename, name,  META_NAME_LEN - 1);
                strncpy(m->title,    name,  META_TITLE_LEN - 1);
                strncpy(m->subdir,   rel,   META_SUBDIR_LEN - 1);
                m->unreadable = (access(child, R_OK) != 0);
                if (!m->unreadable) {
                    m->binary = is_binary_file(child);
                    if (!m->binary) {
                        m->line_count = count_lines(child);
                        read_frontmatter(child, m);
                    }
                }
                count++;

                if (progress_fn && (count - last_flush) >= SCAN_BATCH_SIZE) {
                    progress_fn(files, count, progress_ctx);
                    last_flush = count;
                }
            }
        }
        closedir(d);
    }

done:
    free(bfs_q);
    qsort(files, count, sizeof(FileMeta), cmp_subdir_filename);
    *count_out = count;

    if (disc_out && disc_count_out) {
        *disc_out       = disc_arr;
        *disc_count_out = disc_count;
    } else {
        free(disc_arr);
    }

    return files;
}
