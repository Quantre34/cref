---
baslik: "complex.h - Karmaşık Sayılar"
kategori: "kutuphane"
etiketler: ["complex", "imaginary", "math", "karmaşik-sayilar"]
zorluk: "ileri"
ilgili: ["08-standart-kutuphane-referansi/math-detay.md"]
---

# complex.h - Karmaşık Sayılar

C99 standardı ile gelen `complex.h`, matematiksel ve bilimsel hesaplamalarda kullanılan karmaşık sayıları (reel + sanal kısım) temsil etmek için araçlar sunar.

## 1. Tanımlama

```c
#include <complex.h>

double complex z = 3.0 + 4.0 * I; // 3 + 4i
```
`I` sabiti, sanal birimdir ($\sqrt{-1}$).

---

## 2. Temel Fonksiyonlar

| Fonksiyon | Görevi |
|-----------|--------|
| `creal(z)` | Reel kısmı verir (3.0). |
| `cimag(z)` | Sanal kısmı verir (4.0). |
| `cabs(z)` | Mutlak değer (genlik). |
| `carg(z)` | Faz açısını verir. |
| `conj(z)` | Eşleniğini verir (3 - 4i). |

---

## 3. Trigonometrik ve Üstel

`math.h` içindeki fonksiyonların karmaşık sayı versiyonları `c` ön ekiyle mevcuttur:
- `csin(z)`, `ccos(z)`, `cexp(z)`, `clog(z)`.

---

## 4. Öğrenme Köşesi: Mühendislikte Kullanım

Karmaşık sayılar, elektrik mühendisliğinde alternatif akım (AC) devrelerini ve sinyal işleme algoritmalarını (FFT - Fast Fourier Transform) modellemek için hayati önem taşır.

---

*Yerel ayarlar ve diller:* [locale-detay.md](locale-detay.md)
