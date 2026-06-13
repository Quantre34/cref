#ifndef META_H
#define META_H

#define META_MAX_FILES    2000
#define META_MAX_TAGS     24
#define META_PATH_LEN     512
#define META_NAME_LEN     128
#define META_TITLE_LEN    256
#define META_CAT_LEN      64
#define META_TAG_LEN      64
#define META_DIFF_LEN     32
#define META_SUBDIR_LEN   256   /* relative subdir path from scan root */

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
} FileMeta;

/* Recursively scans dir for files matching any extension in exts[].
   exts is an array of ext_count strings without dots (e.g. "md", "txt").
   Returns file count on success, -1 if dir cannot be opened. */
int scan_dir(const char *dir, FileMeta *files, int max,
             const char * const *exts, int ext_count);

#endif
