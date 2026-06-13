---
baslik: "wchar.h ve Unicode - Geniş Karakterler"
kategori: "kutuphane"
etiketler: ["wchar", "unicode", "utf8", "wprintf", "multibyte"]
zorluk: "ileri"
ilgili: ["08-standart-kutuphane-referansi/string-detay.md"]
---

# wchar.h ve Unicode - Geniş Karakterler

C'nin orijinal `char` tipi sadece 1 byte (256 karakter) tutabilir. Bu, dünya dillerindeki (Çince, Arapça vb.) binlerce karakteri temsil etmek için yetersizdir. `wchar.h`, daha geniş yer kaplayan (**Wide Characters**) karakterlerle çalışmamızı sağlar.

## 1. wchar_t Tipi

Genellikle 2 veya 4 byte yer kaplar.

```c
#include <wchar.h>

wchar_t harf = L'Ω'; // Omega karakteri
wchar_t metin[] = L"Merhaba Dünya";
```
`L` ön eki, derleyiciye bunun bir "Wide String" olduğunu söyler.

---

## 2. Temel Fonksiyonlar

Wide character fonksiyonları genelde `w` harfiyle başlar:

| Normal | Wide Karşılığı |
|--------|----------------|
| `printf` | `wprintf` |
| `strlen` | `wcslen` |
| `strcpy` | `wcscpy` |
| `scanf`  | `wscanf` |

---

## 3. Unicode ve UTF-8 Dönüşümü

Ham byte dizilerini (UTF-8) `wchar_t` dizisine çevirmek için `mbstowcs` (multibyte string to wide character string) kullanılır.

---

## 4. Öğrenme Köşesi: "Portable" Karakterler

C11 ile birlikte kesin boyutlu Unicode tipleri de geldi (`<uchar.h>`):
- `char16_t`: UTF-16 için.
- `char32_t`: UTF-32 için.

---

## 5. StackOverflow Notları: "Windows vs Linux"

`sizeof(wchar_t)` Windows'ta 2 byte iken, Linux'ta genelde 4 byte'tır. Bu yüzden cross-platform projelerde `wchar_t` kullanırken çok dikkatli olunmalıdır. UTF-8 (`char` dizisi) kullanmak genelde daha taşınabilir bir yoldur.

---

## 6. Gerçek Hayat: Emoji Desteği

Modern terminal uygulamalarında gülen yüz (😀) gibi emojileri doğru basabilmek için programın Unicode desteğine ve `wchar.h` fonksiyonlarına ihtiyacı vardır.

---

*Hata raporlama:* [errno-detay.md](errno-detay.md)
