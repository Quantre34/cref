#include "meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

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
                /* first line is body — fall through to snippet capture */
            } else if (strcmp(line, "---") == 0) {
                in_fm   = 0;
                past_fm = 1;
                continue;
            } else {
                /* inside frontmatter: parse key:value */
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
                    char *tok = strtok(buf, ",");
                    meta->tag_count = 0;
                    while (tok && meta->tag_count < META_MAX_TAGS) {
                        char *t = trim(tok);
                        strip_quotes(t);
                        if (*t) {
                            strncpy(meta->tags[meta->tag_count], t, META_TAG_LEN - 1);
                            meta->tag_count++;
                        }
                        tok = strtok(NULL, ",");
                    }
                }
                continue;
            }
        }

        /* Accumulate content snippet (skip blank lines) */
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
    "node_modules", ".git", "dist", "build", ".cache",
    "vendor", "target", ".next", "coverage", NULL
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
    if (ext_count == 0) return 1;   /* 0 = accept all file types */
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

static void scan_recursive(const char *root, const char *rel,
                            FileMeta *files, int *count, int max,
                            const char * const *exts, int ext_count) {
    char full[META_PATH_LEN];
    if (rel[0])
        snprintf(full, sizeof(full), "%s/%s", root, rel);
    else
        strncpy(full, root, sizeof(full) - 1);

    DIR *d = opendir(full);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && *count < max) {
        const char *name = entry->d_name;
        if (name[0] == '.') continue;

        char child[META_PATH_LEN];
        snprintf(child, sizeof(child), "%s/%s", full, name);

        struct stat st;
        if (stat(child, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (should_skip_dir(name)) continue;
            char child_rel[META_SUBDIR_LEN];
            if (rel[0])
                snprintf(child_rel, sizeof(child_rel), "%s/%s", rel, name);
            else
                strncpy(child_rel, name, sizeof(child_rel) - 1);
            scan_recursive(root, child_rel, files, count, max, exts, ext_count);

        } else if (S_ISREG(st.st_mode)) {
            if (!match_ext(name, exts, ext_count)) continue;
            if (should_skip(name)) continue;

            FileMeta *m = &files[*count];
            memset(m, 0, sizeof(*m));
            strncpy(m->path,     child, META_PATH_LEN - 1);
            strncpy(m->filename, name,  META_NAME_LEN - 1);
            strncpy(m->title,    name,  META_TITLE_LEN - 1);
            strncpy(m->subdir,   rel,   META_SUBDIR_LEN - 1);
            m->binary = is_binary_file(child);
            if (!m->binary) {
                m->line_count = count_lines(child);
                read_frontmatter(child, m);
            }
            (*count)++;
        }
    }
    closedir(d);
}

static int cmp_subdir_filename(const void *a, const void *b) {
    const FileMeta *fa = (const FileMeta *)a;
    const FileMeta *fb = (const FileMeta *)b;
    int c = strcmp(fa->subdir, fb->subdir);
    return c ? c : strcmp(fa->filename, fb->filename);
}

int scan_dir(const char *dir, FileMeta *files, int max,
             const char * const *exts, int ext_count) {
    DIR *test = opendir(dir);
    if (!test) return -1;
    closedir(test);

    /* ext_count == 0 means accept all file types (handled in match_ext) */
    if (!exts) { ext_count = 0; }

    int count = 0;
    scan_recursive(dir, "", files, &count, max, exts, ext_count);
    qsort(files, count, sizeof(FileMeta), cmp_subdir_filename);
    return count;
}
