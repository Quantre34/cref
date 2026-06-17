#include "../meta.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int failures = 0;

#define CHECK(expr, msg) do { \
    if (!(expr)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
    else          { printf("  ok: %s\n", msg); } \
} while(0)

static void test_scan_dir(void) {
    printf("scan_dir:\n");

    /* run from project root via Makefile; ref/ is relative to CWD */
    const char *dir = "ref";

    int count = 0;
    FileMeta *files = scan_dir(dir, &count, NULL, 0, -1, NULL, NULL, NULL, NULL, NULL);

    CHECK(files != NULL,  "scan_dir returns non-NULL for ref/");
    CHECK(count  > 0,     "finds at least one file in ref/");

    if (files && count > 0) {
        /* every entry must have a non-empty filename */
        int all_named = 1;
        for (int i = 0; i < count; i++) {
            if (files[i].filename[0] == '\0') { all_named = 0; break; }
        }
        CHECK(all_named, "all scanned files have non-empty filename");

        /* path must contain the filename */
        int paths_ok = 1;
        for (int i = 0; i < count; i++) {
            if (!strstr(files[i].path, files[i].filename)) { paths_ok = 0; break; }
        }
        CHECK(paths_ok, "each file path contains its filename");
    }

    free(files);
}

static void test_scan_ext_filter(void) {
    printf("scan_dir ext filter:\n");

    const char *exts[] = { "md" };
    int count = 0;
    FileMeta *files = scan_dir("ref", &count, exts, 1, -1, NULL, NULL, NULL, NULL, NULL);

    CHECK(files != NULL, "scan with md filter returns non-NULL");
    CHECK(count  > 0,    "finds md files");

    if (files && count > 0) {
        int all_md = 1;
        for (int i = 0; i < count; i++) {
            const char *dot = strrchr(files[i].filename, '.');
            if (!dot || strcmp(dot + 1, "md") != 0) { all_md = 0; break; }
        }
        CHECK(all_md, "ext filter: all results are .md files");
    }

    free(files);
}

static void test_scan_nonexistent(void) {
    printf("scan_dir nonexistent:\n");
    int count = 0;
    FileMeta *files = scan_dir("/nonexistent_path_xyz", &count, NULL, 0,
                               -1, NULL, NULL, NULL, NULL, NULL);
    CHECK(files == NULL, "nonexistent dir returns NULL");
    CHECK(count == 0,    "nonexistent dir sets count to 0");
    free(files);
}

int main(void) {
    test_scan_dir();
    test_scan_ext_filter();
    test_scan_nonexistent();

    if (failures) {
        fprintf(stderr, "\n%d test(s) failed.\n", failures);
        return 1;
    }
    printf("\nAll meta tests passed.\n");
    return 0;
}
