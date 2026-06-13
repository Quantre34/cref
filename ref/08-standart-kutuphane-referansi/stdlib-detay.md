---
baslik: "stdlib.h - Standart Araçlar"
kategori: "kutuphane"
etiketler: ["stdlib", "malloc", "free", "exit", "rand", "qsort"]
zorluk: "tum-seviyeler"
ilgili: ["02-bellek-ve-veri-yonetimi/04-dinamik-bellek-yonetimi.md"]
---

# stdlib.h - Standart Araçlar

Bu kütüphane bellek yönetimi, rastgele sayılar ve süreç kontrolü gibi kritik araçlar sunar. C'nin tip kısıtlamalarına uymak için `void *` ve `size_t` gibi tipleri doğru anlamak gerekir.

## 1. Dinamik Bellek Yönetimi

| Fonksiyon İmzası | Girdi Tipleri | Dönüş Tipi | Açıklama |
|------------------|---------------|------------|----------|
| `void *malloc(size_t size)` | `size_t` | `void *` | Bellek ayırır. |
| `void *calloc(size_t n, size_t size)` | `size_t, size_t` | `void *` | Sıfırlayarak ayırır. |
| `void *realloc(void *ptr, size_t size)` | `void *, size_t` | `void *` | Mevcut yeri büyütür/küçültür. |
| `void free(void *ptr)` | `void *` | `void` | Belleği iade eder. |

---

## 2. Sayısal Dönüştürme ve Arama

| Fonksiyon İmzası | Girdi Tipleri | Dönüş Tipi | Açıklama |
|------------------|---------------|------------|----------|
| `int atoi(const char *str)` | `const char *` | `int` | Metni tam sayıya çevirir. |
| `double atof(const char *str)` | `const char *` | `double` | Metni ondalıklı sayıya çevirir. |
| `void qsort(...)` | `void*, size_t, size_t, int(*)(...)` | `void` | Hızlı sıralama algoritması. |
| `void *bsearch(...)` | `void*, void*, size_t, size_t, int(*)(...)` | `void *` | İkili arama algoritması. |

---

## 3. Süreç ve Rastgelelik

- `void exit(int status)`: Programı sonlandırır.
- `int rand(void)`: 0 ile `RAND_MAX` arası rastgele `int` döner.
- `void srand(unsigned int seed)`: Rastgele sayı üretecini tohumlar.
- `int system(const char *command)`: İşletim sistemi komutu çalıştırır.

---

## 4. Öğrenme Köşesi: `void *` Esnekliği

`malloc` neden `void *` döndürür? Çünkü derleyici sizin o belleği `int` dizisi mi yoksa bir `struct` için mi kullanacağınızı bilemez. `void *` her türlü pointer tipine sessizce (cast gerektirmeden) dönüşebilir.

---

## 5. StackOverflow Notları: `atoi` vs `strtol`

`atoi()` fonksiyonu hata durumunda sadece `0` döner. Eğer metin gerçekten "0" ise hatayla gerçek değeri ayıramazsınız. Profesyonel projelerde hata kontrolü yapabildiği için her zaman `strtol()` (string to long) tercih edilir.

---

*Metin manipülasyonu:* [string-detay.md](string-detay.md)
