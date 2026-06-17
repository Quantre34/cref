#include "../search.h"
#include "../meta.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(expr, msg) do { \
    if (!(expr)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
    else          { printf("  ok: %s\n", msg); } \
} while(0)

static void test_levenshtein(void) {
    printf("levenshtein:\n");
    CHECK(levenshtein("malloc", "malloc") == 0,  "exact match → 0");
    CHECK(levenshtein("malloc", "maloc")  == 1,  "one deletion → 1");
    CHECK(levenshtein("malloc", "mallok") == 1,  "one substitution → 1");
    CHECK(levenshtein("printf", "printg") == 1,  "one substitution → 1");
    CHECK(levenshtein("a",      "malloc") == 99, "length diff >4 → 99");
    CHECK(levenshtein("",       "malloc") == 99, "empty vs long (diff>4) → 99");
    CHECK(levenshtein("MALLOC", "malloc") == 0,  "case-insensitive → 0");
}

static void test_file_score(void) {
    printf("file_score:\n");

    FileMeta m;
    memset(&m, 0, sizeof(m));
    strncpy(m.title,    "malloc",      sizeof(m.title) - 1);
    strncpy(m.filename, "malloc.md",   sizeof(m.filename) - 1);
    strncpy(m.category, "memory",      sizeof(m.category) - 1);
    strncpy(m.tags[0],  "malloc",      sizeof(m.tags[0]) - 1);
    m.tag_count = 1;

    CHECK(file_score(&m, "malloc")  >= 70, "exact title match scores high");
    CHECK(file_score(&m, "memory")  >= 60, "category match scores");
    CHECK(file_score(&m, "xyz_zzz") == 0,  "no match → 0");
    CHECK(file_score(&m, "")        >= 1,  "empty query → nonzero");
    CHECK(file_score(&m, "MALLOC")  >= 70, "case-insensitive query");
    CHECK(file_score(&m, "maloc")   >= 20, "typo gets fuzzy score");
}

int main(void) {
    test_levenshtein();
    test_file_score();

    if (failures) {
        fprintf(stderr, "\n%d test(s) failed.\n", failures);
        return 1;
    }
    printf("\nAll search tests passed.\n");
    return 0;
}
