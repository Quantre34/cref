#ifndef META_H
#define META_H

#define SCAN_BATCH_SIZE    400   /* progress callback interval during scan */
#define META_MAX_TAGS      24
#define META_PATH_LEN      512
#define META_NAME_LEN      128
#define META_TITLE_LEN     256
#define META_CAT_LEN       64
#define META_TAG_LEN       64
#define META_DIFF_LEN      32
#define META_SUBDIR_LEN    256   /* relative subdir path from scan root */

typedef struct {
    char path[META_PATH_LEN];
    char filename[META_NAME_LEN];
    char title[META_TITLE_LEN];
    char category[META_CAT_LEN];
    char tags[META_MAX_TAGS][META_TAG_LEN];
    int  tag_count;
    char difficulty[META_DIFF_LEN];
    char subdir[META_SUBDIR_LEN]; /* "" = root level */
    int  line_count;
    char snippet[256];  /* first ~200 chars of non-frontmatter content */
    int  binary;        /* 1 if file has NUL bytes in first 512B */
    int  unreadable;    /* 1 if access(path, R_OK) fails */
} FileMeta;

/* Called periodically during scan with files found so far.
   Do NOT free or realloc the array; the scanner owns it. */
typedef void (*ScanProgressFn)(const FileMeta *files, int count, void *ctx);

/* Dynamically scans dir (BFS order) for files matching any extension in exts[].
   ext_count == 0 accepts all file types.  progress_fn may be NULL.
   Returns heap-allocated FileMeta array (caller must free), sets *count_out.
   Returns NULL if dir cannot be opened. */
FileMeta *scan_dir(const char *dir, int *count_out,
                   const char * const *exts, int ext_count,
                   ScanProgressFn progress_fn, void *progress_ctx,
                   volatile int *cancel);

#endif
