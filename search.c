#include "search.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static int imin3(int a, int b, int c) {
    int ab = a < b ? a : b;
    return ab < c ? ab : c;
}

/* Lowercase for ASCII only — leave UTF-8 bytes untouched */
static int c_lower(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

int levenshtein(const char *a, const char *b) {
    int la = (int)strlen(a);
    int lb = (int)strlen(b);

    int diff = la - lb;
    if (diff < 0) diff = -diff;
    if (diff > 4) return 99;

    int dp[la + 1][lb + 1];

    for (int i = 0; i <= la; i++) dp[i][0] = i;
    for (int j = 0; j <= lb; j++) dp[0][j] = j;

    for (int i = 1; i <= la; i++) {
        for (int j = 1; j <= lb; j++) {
            int cost = (c_lower((unsigned char)a[i-1]) ==
                        c_lower((unsigned char)b[j-1])) ? 0 : 1;
            dp[i][j] = imin3(dp[i-1][j] + 1,
                             dp[i][j-1] + 1,
                             dp[i-1][j-1] + cost);
        }
    }
    return dp[la][lb];
}

/* Score one keyword against one text field.
   is_tag=1 uses tighter scoring (exact tag match ranks highest). */
static int word_field_score(const char *word, const char *text, int is_tag) {
    if (!*word || !*text) return 0;

    char wl[META_TAG_LEN], tl[META_TITLE_LEN];
    int i;
    for (i = 0; word[i] && i < (int)sizeof(wl) - 1; i++)
        wl[i] = (char)c_lower((unsigned char)word[i]);
    wl[i] = '\0';

    for (i = 0; text[i] && i < (int)sizeof(tl) - 1; i++)
        tl[i] = (char)c_lower((unsigned char)text[i]);
    tl[i] = '\0';

    if (strcmp(tl, wl) == 0)  return is_tag ? 100 : 90;
    if (strstr(tl, wl))       return is_tag ?  70 : 60;

    /* Token-by-token fuzzy match */
    char buf[META_TITLE_LEN];
    strncpy(buf, tl, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int   best = 0;
    char *save = NULL;
    char *tok  = strtok_r(buf, " -_/.", &save);
    while (tok) {
        int d = levenshtein(wl, tok);
        if      (d == 1 && best < 40) best = 40;
        else if (d == 2 && best < 20) best = 20;
        tok = strtok_r(NULL, " -_/.", &save);
    }
    return best;
}

static int word_field_score_plain(const char *word, const char *text) {
    return word_field_score(word, text, 0);
}

int file_score(const FileMeta *meta, const char *query) {
    if (!query || !*query) return 1;

    char buf[256];
    strncpy(buf, query, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int   total   = 0;
    int   word_n  = 0;
    char *outer   = NULL;

    char *word = strtok_r(buf, " ", &outer);
    while (word) {
        word_n++;
        int best = 0, s;

        s = word_field_score_plain(word, meta->title);
        if (s > best) best = s;

        s = word_field_score_plain(word, meta->filename);
        if (s > best) best = s;

        s = word_field_score_plain(word, meta->category);
        if (s > best) best = s;

        s = word_field_score_plain(word, meta->difficulty);
        if (s > best) best = s;

        if (meta->snippet[0]) {
            s = word_field_score_plain(word, meta->snippet);
            if (s > best) best = s;
        }

        for (int i = 0; i < meta->tag_count; i++) {
            s = word_field_score(word, meta->tags[i], 1);
            if (s > best) best = s;
        }

        if (best == 0) return 0;
        total += best;

        word = strtok_r(NULL, " ", &outer);
    }

    return word_n > 0 ? total / word_n : 0;
}

typedef struct { int idx; int score; } ScoreEntry;

static int cmp_score(const void *a, const void *b) {
    return ((const ScoreEntry *)b)->score - ((const ScoreEntry *)a)->score;
}

int search(const FileMeta *files, int n, const char *query,
           int *indices, int max) {
    if (n <= 0 || max <= 0) return 0;

    ScoreEntry *results = malloc(n * sizeof(ScoreEntry));
    if (!results) return 0;
    int count = 0;

    for (int i = 0; i < n; i++) {
        int s = file_score(&files[i], query);
        if (s > 0) {
            results[count].idx   = i;
            results[count].score = s;
            count++;
        }
    }

    qsort(results, count, sizeof(ScoreEntry), cmp_score);

    int ret = count < max ? count : max;
    for (int i = 0; i < ret; i++) indices[i] = results[i].idx;
    free(results);
    return ret;
}

int find_suggestion(const FileMeta *files, int n, const char *query,
                    char *suggestion, int suggestion_len) {
    if (!query || !*query) return 0;

    char sq[META_TAG_LEN];
    int i;
    for (i = 0; query[i] && i < (int)sizeof(sq) - 1; i++)
        sq[i] = (char)c_lower((unsigned char)query[i]);
    sq[i] = '\0';

    int         best_dist = 99;
    const char *best      = NULL;

    for (int fi = 0; fi < n; fi++) {
        for (int ti = 0; ti < files[fi].tag_count; ti++) {
            int d = levenshtein(sq, files[fi].tags[ti]);
            if (d < best_dist) { best_dist = d; best = files[fi].tags[ti]; }
        }
        int d = levenshtein(sq, files[fi].category);
        if (d < best_dist) { best_dist = d; best = files[fi].category; }
    }

    if (best_dist <= 2 && best) {
        strncpy(suggestion, best, suggestion_len - 1);
        suggestion[suggestion_len - 1] = '\0';
        return best_dist;
    }
    return 0;
}
