#ifndef NOTES_H
#define NOTES_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stddef.h>

#define NOTE_ID_LEN       33    /* 32 hex chars + NUL */
#define NOTE_TITLE_LEN    256
#define NOTE_CAT_LEN      64
#define NOTE_MAX_CATS     64
#define NOTE_MAX_NOTES    1024

typedef enum { NOTE_TEXT = 0, NOTE_TODO = 1 } NoteType;

typedef struct {
    char id[NOTE_ID_LEN];
    char name[NOTE_CAT_LEN];
} NoteCat;

typedef struct {
    char          id[NOTE_ID_LEN];
    char          cat_id[NOTE_ID_LEN];
    char          title[NOTE_TITLE_LEN];
    NoteType      type;
    int           has_pw;           /* 1 = Layer 2 user password active */
    unsigned char nonce[24];        /* per-note random nonce (XSalsa20) */
    unsigned char pw_salt[16];      /* Argon2id salt (only if has_pw) */
    unsigned char *ciphertext;      /* heap, NULL for new empty notes */
    size_t        ct_len;
    char         *content;          /* heap plaintext, NULL = not decrypted */
    int           content_dirty;
} Note;

typedef struct {
    char          vault_path[512];
    NoteCat       cats[NOTE_MAX_CATS];
    int           cat_count;
    Note          notes[NOTE_MAX_NOTES];
    int           note_count;
    unsigned char device_key[32];
    int           loaded;
} NoteVault;

/* Open (load or create) vault. Returns 0 on success, -2 wrong device. */
int      vault_open(NoteVault *v);

/* Write vault to disk. Returns 0 on success. */
int      vault_save(NoteVault *v);

/* Free all heap resources and zero sensitive memory. */
void     vault_close(NoteVault *v);

/* Add a category. Returns pointer or NULL on overflow. */
NoteCat *vault_add_cat(NoteVault *v, const char *name);

/* Add a new empty note. Returns index or -1 on overflow. */
int      vault_add_note(NoteVault *v, const char *cat_id,
                        const char *title, NoteType type);

/* Delete note by index, shifting array. Returns 0 on success. */
int      vault_delete_note(NoteVault *v, int idx);

/* Decrypt note content into note->content.
   user_pw: NULL or "" = Layer 1 only.
   Returns 0 ok, -1 error, -2 wrong password. */
int      vault_decrypt_note(NoteVault *v, int idx, const char *user_pw);

/* Encrypt note->content into note->ciphertext.
   user_pw: NULL or "" = keep/clear Layer 2 unchanged.
   user_pw non-empty = set/update Layer 2 password. */
int      vault_encrypt_note(NoteVault *v, int idx, const char *user_pw);

/* Remove Layer 2 password from note (re-encrypts with device key only). */
int      vault_clear_note_pw(NoteVault *v, int idx);

typedef struct {
    int  note_idx;
    int  line_no;
    char line[256];
} NoteHit;

/* Search unencrypted (has_pw=0) note content. Returns hit count. */
int vault_search(NoteVault *v, const char *q, NoteHit *hits, int max);

#endif
