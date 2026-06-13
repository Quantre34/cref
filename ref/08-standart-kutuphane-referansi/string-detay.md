---
baslik: "string.h - Metin ve Bellek İşlemleri"
kategori: "kutuphane"
etiketler: ["string", "strlen", "strcpy", "memcpy", "memset"]
zorluk: "tum-seviyeler"
ilgili: ["02-bellek-ve-veri-yonetimi/03-string-manipulasyonu.md"]
---

# string.h - Metin ve Bellek İşlemleri

C'de metinler `char` dizileridir. Bu kütüphane, tip güvenliğine dikkat ederek bu diziler üzerinde işlem yapmanızı sağlar.

## 1. Metin İşlemleri (String)

| Fonksiyon İmzası | Girdi Tipleri | Dönüş Tipi | Açıklama |
|------------------|---------------|------------|----------|
| `size_t strlen(const char *s)` | `char *` | `size_t` | Metin uzunluğu (`\0` hariç). |
| `char *strcpy(char *dest, const char *src)` | `char*, char*` | `char *` | Kopyalar. |
| `char *strncpy(char *dest, const char *src, size_t n)` | `char*, char*, size_t` | `char *` | **Güvenli:** `n` kadar kopyalar. |
| `int strcmp(const char *s1, const char *s2)` | `char*, char*` | `int` | Karşılaştırır (0 ise eşittir). |
| `char *strcat(char *dest, const char *src)` | `char*, char*` | `char *` | Uca ekler. |
| `char *strstr(const char *haystack, const char *needle)` | `char*, char*` | `char *` | İçinde metin arar. |

---

## 2. Bellek İşlemleri (Memory)

| Fonksiyon İmzası | Girdi Tipleri | Dönüş Tipi | Açıklama |
|------------------|---------------|------------|----------|
| `void *memset(void *s, int c, size_t n)` | `void*, int, size_t` | `void *` | Belleği karakterle doldurur. |
| `void *memcpy(void *dest, const void *src, size_t n)` | `void*, void*, size_t` | `void *` | Belleği kopyalar. |
| `void *memmove(void *dest, const void *src, size_t n)` | `void*, void*, size_t` | `void *` | **Güvenli:** Çakışan bölgeleri kopyalar. |
| `int memcmp(const void *s1, const void *s2, size_t n)` | `void*, void*, size_t` | `int` | Belleği karşılaştırır. |

---

## 3. Öğrenme Köşesi: `memcpy` vs `memmove`

Eğer kopyalanacak kaynak ve hedef bölge bellekte **üst üste biniyorsa** (overlap), `memcpy` hatalı sonuç verebilir. Bu durumda `memmove` kullanmalısınız.

---

## 4. StackOverflow Notları: `strtok` Tehlikesi

`strtok` fonksiyonu orijinal metni **değiştirir** (aralara `\0` koyar). Çok kanallı (multi-threaded) programlarda `strtok_r` (reentrant) versiyonu kullanılmalıdır.

---

*Matematiksel hesaplamalar:* [math-detay.md](math-detay.md)
