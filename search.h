#ifndef SEARCH_H
#define SEARCH_H

#include "meta.h"

/* Levenshtein (edit) distance between two strings */
int levenshtein(const char *a, const char *b);

/* Relevance score of a single file against a query (0 = no match) */
int file_score(const FileMeta *meta, const char *query);

/* Filter files by query, write sorted indices into `indices`.
   Returns the number of results. */
int search(const FileMeta *files, int n, const char *query,
           int *indices, int max);

/* Find the closest tag/category to a misspelled query.
   Fills `suggestion` and returns distance if <= 2, otherwise 0. */
int find_suggestion(const FileMeta *files, int n, const char *query,
                    char *suggestion, int suggestion_len);

#endif
