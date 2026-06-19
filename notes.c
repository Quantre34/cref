#include "notes.h"

#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef __APPLE__
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

/* ------------------------------------------------------------------ */
/* Device serial                                                        */
/* ------------------------------------------------------------------ */

static int get_device_serial(char *buf, size_t len) {
    buf[0] = '\0';
#ifdef __APPLE__
    io_service_t svc = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceMatching("IOPlatformExpertDevice"));
    if (!svc) return -1;
    CFStringRef s = (CFStringRef)IORegistryEntryCreateCFProperty(
        svc, CFSTR("IOPlatformSerialNumber"), kCFAllocatorDefault, 0);
    IOObjectRelease(svc);
    if (!s) return -1;
    Boolean ok = CFStringGetCString(s, buf, (CFIndex)len, kCFStringEncodingUTF8);
    CFRelease(s);
    return ok ? 0 : -1;
#else
    const char *paths[] = {
        "/etc/machine-id",
        "/var/lib/dbus/machine-id",
        "/sys/class/dmi/id/product_uuid",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "r");
        if (!f) continue;
        if (fgets(buf, (int)len, f)) {
            char *nl = strchr(buf, '\n');
            if (nl) *nl = '\0';
            fclose(f);
            return buf[0] ? 0 : -1;
        }
        fclose(f);
    }
    return -1;
#endif
}

/* ------------------------------------------------------------------ */
/* devkey file (~/.config/cref/devkey)                                 */
/* ------------------------------------------------------------------ */

static void devkey_path(char *out, size_t len) {
    const char *home = getenv("HOME");
    snprintf(out, len, "%s/.config/cref/devkey",
             home ? home : "/tmp");
}

static void mkdir_p(const char *path) {
    char tmp[512];
    strncpy(tmp, path, sizeof(tmp) - 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0700);
            *p = '/';
        }
    }
    mkdir(tmp, 0700);
}

static int load_devkey(unsigned char key[32]) {
    char path[512];
    devkey_path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    int ok = ((int)fread(key, 1, 32, f) == 32);
    fclose(f);
    return ok ? 0 : -1;
}

static int save_devkey(const unsigned char key[32]) {
    char path[512];
    devkey_path(path, sizeof(path));

    /* Ensure ~/.config/cref/ exists */
    char dir[512];
    strncpy(dir, path, sizeof(dir) - 1);
    char *sl = strrchr(dir, '/');
    if (sl) { *sl = '\0'; mkdir_p(dir); }

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return -1;
    int ok = ((int)write(fd, key, 32) == 32);
    close(fd);
    return ok ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Device key derivation: BLAKE2b(serial || devkey)                    */
/* ------------------------------------------------------------------ */

static int derive_device_key(unsigned char key[32]) {
    if (sodium_init() < 0) return -1;

    char serial[256] = "";
    get_device_serial(serial, sizeof(serial));

    unsigned char devkey[32];
    if (load_devkey(devkey) != 0) {
        randombytes_buf(devkey, 32);
        if (save_devkey(devkey) != 0) {
            sodium_memzero(devkey, 32);
            return -1;
        }
    }

    size_t slen = strlen(serial);
    size_t total = slen + 32;
    unsigned char *ikm = malloc(total);
    if (!ikm) { sodium_memzero(devkey, 32); return -1; }
    memcpy(ikm, serial, slen);
    memcpy(ikm + slen, devkey, 32);

    crypto_generichash(key, 32, ikm, total, NULL, 0);

    sodium_memzero(ikm, total);
    sodium_memzero(devkey, 32);
    free(ikm);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static void pu32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t gu32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void gen_id(char out[NOTE_ID_LEN]) {
    unsigned char raw[16];
    randombytes_buf(raw, 16);
    for (int i = 0; i < 16; i++)
        snprintf(out + i * 2, 3, "%02x", raw[i]);
    out[32] = '\0';
}

/* ------------------------------------------------------------------ */
/* Vault file format                                                    */
/*                                                                      */
/* HEADER (44 bytes):                                                   */
/*   magic[4]        = "CRNV"                                          */
/*   version[4]      = 1 (uint32 LE)                                   */
/*   device_check[32]= generichash(device_key, "cref-v1")             */
/*   meta_len[4]     = uint32 LE                                       */
/*                                                                      */
/* METADATA (meta_len bytes, plain text):                               */
/*   "CAT\t<id>\t<name>\n"                                             */
/*   "NOTE\t<id>\t<cat_id>\t<type>\t<has_pw>\t<title>\n"              */
/*                                                                      */
/* CONTENT BLOCKS (one per note in metadata order):                    */
/*   ct_len[4]   uint32 LE                                             */
/*   nonce[24]                                                          */
/*   pw_salt[16]                                                        */
/*   ciphertext[ct_len]                                                 */
/* ------------------------------------------------------------------ */

#define VAULT_MAGIC    "CRNV"
#define VAULT_VERSION  1
#define DEVICE_CHECK_MSG "cref-v1"

/* ------------------------------------------------------------------ */
/* vault_open                                                           */
/* ------------------------------------------------------------------ */

int vault_open(NoteVault *v) {
    if (sodium_init() < 0) return -1;

    if (!v->vault_path[0]) {
        const char *home = getenv("HOME");
        snprintf(v->vault_path, sizeof(v->vault_path),
                 "%s/.local/share/cref/notes.vault",
                 home ? home : "/tmp");
    }

    if (derive_device_key(v->device_key) != 0) return -1;

    FILE *f = fopen(v->vault_path, "rb");
    if (!f) {
        /* New vault */
        vault_add_cat(v, "General");
        v->loaded = 1;
        return 0;
    }

    uint8_t hdr[44];
    if (fread(hdr, 1, 44, f) != 44) { fclose(f); return -1; }

    if (memcmp(hdr, VAULT_MAGIC, 4) != 0) { fclose(f); return -1; }

    /* Verify this is our device */
    uint8_t expected[32];
    crypto_generichash(expected, 32,
        (const unsigned char *)DEVICE_CHECK_MSG,
        sizeof(DEVICE_CHECK_MSG) - 1,
        v->device_key, 32);
    if (sodium_memcmp(hdr + 8, expected, 32) != 0) {
        fclose(f);
        return -2;
    }

    uint32_t meta_len = gu32(hdr + 40);

    char *meta = malloc(meta_len + 1);
    if (!meta) { fclose(f); return -1; }
    if (fread(meta, 1, meta_len, f) != meta_len) {
        free(meta); fclose(f); return -1;
    }
    meta[meta_len] = '\0';

    /* Parse metadata lines */
    char *line = meta;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char *t1 = strchr(line, '\t');
        if (!t1) { line = nl ? nl + 1 : NULL; continue; }
        *t1++ = '\0';

        if (strcmp(line, "CAT") == 0 && v->cat_count < NOTE_MAX_CATS) {
            char *t2 = strchr(t1, '\t');
            if (t2) {
                *t2++ = '\0';
                NoteCat *c = &v->cats[v->cat_count++];
                strncpy(c->id,   t1, NOTE_ID_LEN - 1);
                strncpy(c->name, t2, NOTE_CAT_LEN - 1);
            }
        } else if (strcmp(line, "NOTE") == 0 && v->note_count < NOTE_MAX_NOTES) {
            /* NOTE \t id \t cat_id \t type \t has_pw \t title */
            char *fields[5];
            int fi = 0;
            char *p = t1;
            while (fi < 5 && p) {
                fields[fi++] = p;
                char *nx = strchr(p, '\t');
                if (nx) *nx++ = '\0';
                p = nx;
            }
            if (fi == 5) {
                Note *n = &v->notes[v->note_count++];
                memset(n, 0, sizeof(*n));
                strncpy(n->id,     fields[0], NOTE_ID_LEN - 1);
                strncpy(n->cat_id, fields[1], NOTE_ID_LEN - 1);
                n->type   = (NoteType)atoi(fields[2]);
                n->has_pw = atoi(fields[3]);
                strncpy(n->title, fields[4], NOTE_TITLE_LEN - 1);
            }
        }

        line = nl ? nl + 1 : NULL;
    }
    free(meta);

    /* Read content blocks */
    for (int i = 0; i < v->note_count; i++) {
        Note *n = &v->notes[i];
        uint8_t blk[44];
        if (fread(blk, 1, 44, f) != 44) break;
        uint32_t ct_len = gu32(blk);
        memcpy(n->nonce,   blk + 4, 24);
        memcpy(n->pw_salt, blk + 28, 16);
        if (ct_len > 0) {
            n->ciphertext = malloc(ct_len);
            if (!n->ciphertext) break;
            if (fread(n->ciphertext, 1, ct_len, f) != ct_len) {
                free(n->ciphertext);
                n->ciphertext = NULL;
                break;
            }
            n->ct_len = ct_len;
        }
    }
    fclose(f);

    /* Auto-decrypt notes that have no user password */
    for (int i = 0; i < v->note_count; i++) {
        if (!v->notes[i].has_pw)
            vault_decrypt_note(v, i, NULL);
    }

    v->loaded = 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* vault_save                                                           */
/* ------------------------------------------------------------------ */

int vault_save(NoteVault *v) {
    /* Ensure ~/.local/share/cref/ exists */
    char dir[512];
    strncpy(dir, v->vault_path, sizeof(dir) - 1);
    char *sl = strrchr(dir, '/');
    if (sl) { *sl = '\0'; mkdir_p(dir); }

    /* Build metadata */
    size_t mcap = 65536;
    char *meta = malloc(mcap);
    if (!meta) return -1;
    size_t mlen = 0;

    for (int i = 0; i < v->cat_count; i++) {
        int w = snprintf(meta + mlen, mcap - mlen, "CAT\t%s\t%s\n",
                         v->cats[i].id, v->cats[i].name);
        if (w > 0) mlen += (size_t)w;
    }
    for (int i = 0; i < v->note_count; i++) {
        Note *n = &v->notes[i];
        /* Sanitize title: replace tabs and newlines with space */
        char safe[NOTE_TITLE_LEN];
        int j = 0;
        for (int k = 0; n->title[k] && j < NOTE_TITLE_LEN - 1; k++) {
            char c = n->title[k];
            safe[j++] = (c == '\t' || c == '\n') ? ' ' : c;
        }
        safe[j] = '\0';
        int w = snprintf(meta + mlen, mcap - mlen,
                         "NOTE\t%s\t%s\t%d\t%d\t%s\n",
                         n->id, n->cat_id, (int)n->type, n->has_pw, safe);
        if (w > 0) mlen += (size_t)w;
    }

    /* Build header */
    uint8_t hdr[44];
    memcpy(hdr, VAULT_MAGIC, 4);
    pu32(hdr + 4, VAULT_VERSION);
    crypto_generichash(hdr + 8, 32,
        (const unsigned char *)DEVICE_CHECK_MSG,
        sizeof(DEVICE_CHECK_MSG) - 1,
        v->device_key, 32);
    pu32(hdr + 40, (uint32_t)mlen);

    /* Write to .tmp, then rename */
    char tmp[520];
    snprintf(tmp, sizeof(tmp), "%s.tmp", v->vault_path);
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(meta); return -1; }

    int ok = 1;
    ok &= (fwrite(hdr, 1, 44, f) == 44);
    ok &= (fwrite(meta, 1, mlen, f) == mlen);
    free(meta);

    for (int i = 0; i < v->note_count && ok; i++) {
        Note *n = &v->notes[i];
        uint8_t blk[44];
        pu32(blk, (uint32_t)n->ct_len);
        memcpy(blk + 4, n->nonce, 24);
        memcpy(blk + 28, n->pw_salt, 16);
        ok &= (fwrite(blk, 1, 44, f) == 44);
        if (n->ct_len > 0 && n->ciphertext)
            ok &= (fwrite(n->ciphertext, 1, n->ct_len, f) == n->ct_len);
    }

    fclose(f);
    if (!ok) { remove(tmp); return -1; }

    rename(tmp, v->vault_path);
    chmod(v->vault_path, 0600);
    return 0;
}

/* ------------------------------------------------------------------ */
/* vault_close                                                          */
/* ------------------------------------------------------------------ */

void vault_close(NoteVault *v) {
    for (int i = 0; i < v->note_count; i++) {
        Note *n = &v->notes[i];
        if (n->ciphertext) { free(n->ciphertext); n->ciphertext = NULL; }
        if (n->content) {
            sodium_memzero(n->content, strlen(n->content));
            free(n->content);
            n->content = NULL;
        }
    }
    sodium_memzero(v->device_key, 32);
    v->loaded = 0;
}

/* ------------------------------------------------------------------ */
/* vault_add_cat                                                        */
/* ------------------------------------------------------------------ */

NoteCat *vault_add_cat(NoteVault *v, const char *name) {
    if (v->cat_count >= NOTE_MAX_CATS) return NULL;
    NoteCat *c = &v->cats[v->cat_count++];
    gen_id(c->id);
    strncpy(c->name, name, NOTE_CAT_LEN - 1);
    return c;
}

/* ------------------------------------------------------------------ */
/* vault_add_note                                                       */
/* ------------------------------------------------------------------ */

int vault_add_note(NoteVault *v, const char *cat_id,
                   const char *title, NoteType type) {
    if (v->note_count >= NOTE_MAX_NOTES) return -1;
    int idx = v->note_count++;
    Note *n = &v->notes[idx];
    memset(n, 0, sizeof(*n));
    gen_id(n->id);
    if (cat_id && cat_id[0])
        strncpy(n->cat_id, cat_id, NOTE_ID_LEN - 1);
    strncpy(n->title, title, NOTE_TITLE_LEN - 1);
    n->type = type;
    n->content = calloc(1, 1); /* empty string */
    return idx;
}

/* ------------------------------------------------------------------ */
/* vault_delete_note                                                    */
/* ------------------------------------------------------------------ */

int vault_delete_note(NoteVault *v, int idx) {
    if (idx < 0 || idx >= v->note_count) return -1;
    Note *n = &v->notes[idx];
    free(n->ciphertext);
    if (n->content) {
        sodium_memzero(n->content, strlen(n->content));
        free(n->content);
    }
    for (int i = idx; i < v->note_count - 1; i++)
        v->notes[i] = v->notes[i + 1];
    memset(&v->notes[v->note_count - 1], 0, sizeof(Note));
    v->note_count--;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Per-note key: device_key XOR Argon2id(user_pw) if has_pw           */
/* ------------------------------------------------------------------ */

static void make_note_key(const NoteVault *v, const Note *n,
                          const char *user_pw, unsigned char key[32]) {
    if (n->has_pw && user_pw && *user_pw) {
        unsigned char ukey[32];
        if (crypto_pwhash(ukey, 32,
                          user_pw, strlen(user_pw),
                          n->pw_salt,
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE,
                          crypto_pwhash_ALG_ARGON2ID13) != 0) {
            /* Out of memory — fall back to device_key only */
            memcpy(key, v->device_key, 32);
            return;
        }
        for (int i = 0; i < 32; i++)
            key[i] = v->device_key[i] ^ ukey[i];
        sodium_memzero(ukey, 32);
    } else {
        memcpy(key, v->device_key, 32);
    }
}

/* ------------------------------------------------------------------ */
/* vault_decrypt_note                                                   */
/* ------------------------------------------------------------------ */

int vault_decrypt_note(NoteVault *v, int idx, const char *user_pw) {
    if (idx < 0 || idx >= v->note_count) return -1;
    Note *n = &v->notes[idx];

    if (n->content) return 0; /* already in memory */

    if (!n->ciphertext || n->ct_len == 0) {
        n->content = calloc(1, 1);
        return 0;
    }

    if (n->ct_len < crypto_secretbox_MACBYTES) return -1;

    unsigned char key[32];
    make_note_key(v, n, user_pw, key);

    size_t mlen = n->ct_len - crypto_secretbox_MACBYTES;
    char *plain = malloc(mlen + 1);
    if (!plain) { sodium_memzero(key, 32); return -1; }

    int rc = crypto_secretbox_open_easy(
        (unsigned char *)plain, n->ciphertext, n->ct_len, n->nonce, key);
    sodium_memzero(key, 32);

    if (rc != 0) {
        free(plain);
        /* Backward-compat: old empty notes were encrypted without a proper MAC.
           Treat them as empty content rather than a decryption error. */
        if (mlen == 0 && !n->has_pw) {
            n->content = calloc(1, 1);
            return 0;
        }
        return n->has_pw ? -2 : -1;
    }
    plain[mlen] = '\0';
    n->content = plain;
    return 0;
}

/* ------------------------------------------------------------------ */
/* vault_encrypt_note                                                   */
/* ------------------------------------------------------------------ */

int vault_encrypt_note(NoteVault *v, int idx, const char *user_pw) {
    if (idx < 0 || idx >= v->note_count) return -1;
    Note *n = &v->notes[idx];

    const char *plain = n->content ? n->content : "";
    size_t mlen = strlen(plain);
    size_t ct_len = mlen + crypto_secretbox_MACBYTES;

    unsigned char *ct = malloc(ct_len ? ct_len : 1);
    if (!ct) return -1;

    randombytes_buf(n->nonce, 24);

    /* Layer 2: set password if caller provides one */
    if (user_pw && *user_pw) {
        n->has_pw = 1;
        randombytes_buf(n->pw_salt, 16);
    }

    unsigned char key[32];
    make_note_key(v, n, (n->has_pw ? user_pw : NULL), key);

    crypto_secretbox_easy(ct, (const unsigned char *)plain, mlen, n->nonce, key);
    sodium_memzero(key, 32);

    free(n->ciphertext);
    n->ciphertext = ct;
    n->ct_len = ct_len;
    n->content_dirty = 0;
    return 0;
}

/* ------------------------------------------------------------------ */
/* vault_clear_note_pw                                                  */
/* ------------------------------------------------------------------ */

int vault_clear_note_pw(NoteVault *v, int idx) {
    if (idx < 0 || idx >= v->note_count) return -1;
    Note *n = &v->notes[idx];
    if (!n->content) return -1; /* must be decrypted first */
    n->has_pw = 0;
    memset(n->pw_salt, 0, 16);
    return vault_encrypt_note(v, idx, NULL);
}

/* ------------------------------------------------------------------ */
/* vault_search                                                         */
/* ------------------------------------------------------------------ */

int vault_search(NoteVault *v, const char *q, NoteHit *hits, int max) {
    if (!q || !*q || max <= 0) return 0;
    int count = 0;
    size_t qlen = strlen(q);

    for (int i = 0; i < v->note_count && count < max; i++) {
        Note *n = &v->notes[i];
        if (n->has_pw || !n->content) continue;

        const char *p = n->content;
        int lineno = 1;
        while (*p && count < max) {
            const char *nl = strchr(p, '\n');
            size_t llen = nl ? (size_t)(nl - p) : strlen(p);
            for (size_t j = 0; j + qlen <= llen; j++) {
                int match = 1;
                for (size_t k = 0; k < qlen; k++) {
                    if (tolower((unsigned char)p[j + k]) !=
                        tolower((unsigned char)q[k])) { match = 0; break; }
                }
                if (match) {
                    NoteHit *h = &hits[count++];
                    h->note_idx = i;
                    h->line_no  = lineno;
                    size_t cp = llen < 255 ? llen : 255;
                    memcpy(h->line, p, cp);
                    h->line[cp] = '\0';
                    break;
                }
            }
            p = nl ? nl + 1 : p + llen;
            if (!nl) break;
            lineno++;
        }
    }
    return count;
}
