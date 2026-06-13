---
baslik: "assert, stdbool ve stddef - Yardımcı Headerlar"
kategori: "kutuphane"
etiketler: ["assert", "bool", "null", "size_t", "ptrdiff_t"]
zorluk: "baslangic"
ilgili: ["01-temel-programlama/03-veri-tipleri-derinlemesine.md"]
---

# assert, stdbool ve stddef - Yardımcı Headerlar

Bu küçük ama hayati kütüphaneler, kodun doğruluğunu kontrol etmemizi ve standart tanımları kullanmamızı sağlar.

## 1. assert.h (Doğrulama)

Geliştirme aşamasında "bu değişken mutlaka pozitif olmalı" gibi varsayımları kontrol eder. Eğer koşul yanlışsa programı durdurur ve hatayı söyler.

```c
#include <assert.h>
assert(yas >= 18); // yas 18'den küçükse program çöker.
```
**Not:** `#define NDEBUG` yazarsanız tüm assertler koddan silinir (Performans için production modunda yapılır).

---

## 2. stdbool.h (Mantıksal Tip)

C99 öncesi `bool` diye bir tip yoktu. Bu kütüphane bize `bool`, `true` (1) ve `false` (0) kelimelerini kazandırır.

```c
#include <stdbool.h>
bool bitti_mi = false;
```

---

## 3. stddef.h (Temel Tanımlar)

C'nin en temel sabitlerini içerir:
- `NULL`: Boş adres (0).
- `size_t`: `sizeof` operatörünün döndürdüğü tip (İşaretsiz tam sayı).
- `ptrdiff_t`: İki pointer arasındaki farkın tipi (İşaretli tam sayı).
- `offsetof`: Bir struct içindeki bir elemanın kaçıncı byte'tan başladığını verir.

---

## 4. iso646.h (Sembol Alternatifleri)

Eğer klavyenizde `&`, `|`, `!` gibi tuşlar yoksa (veya okumayı kolaylaştırmak için):
- `&&` yerine `and`
- `||` yerine `or`
- `!` yerine `not` yazmanızı sağlar.

---

## 5. Öğrenme Köşesi: "Static Assert" vs "Runtime Assert"

- `assert()`: Program **çalışırken** kontrol eder.
- `_Static_assert`: Program **derlenirken** kontrol eder (C11).

---

## 6. Gerçek Hayat: Savunmacı Programlama (Defensive Programming)

Büyük projelerde fonksiyonların başına `assert(ptr != NULL);` koymak, hatanın nerede olduğunu bulmayı 10 kat hızlandırır. Hata olduğu anda programın durması, sessizce yanlış hesap yapmasından çok daha iyidir.

---

*Hata yönetimi:* [07-ileri-seviye-c/04-hata-yonetimi-setjmp-longjmp.md](../07-ileri-seviye-c/04-hata-yonetimi-setjmp-longjmp.md)
