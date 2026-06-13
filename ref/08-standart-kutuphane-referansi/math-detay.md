---
baslik: "math.h - Matematiksel Fonksiyonlar"
kategori: "kutuphane"
etiketler: ["math", "sqrt", "pow", "sin", "cos", "abs"]
zorluk: "baslangic"
ilgili: ["01-temel-programlama/03-veri-tipleri-derinlemesine.md"]
---

# math.h - Matematiksel Fonksiyonlar

Karmaşık matematiksel hesaplamalar için kullanılır. C, tip kısıtlı bir dil olduğu için bu fonksiyonların hangi tipi alıp hangi tipi döndürdüğü kritiktir. Çoğu standart fonksiyon `double` ile çalışır; `float` için sonuna `f` eklenmiş halleri (örn: `sqrtf`) kullanılır.

## 1. Temel Fonksiyonlar

| Fonksiyon İmzası | Açıklama | Girdi Tipi | Dönüş Tipi |
|------------------|----------|------------|------------|
| `double sqrt(double x)` | Karekök $\sqrt{x}$ | `double` | `double` |
| `double pow(double x, double y)` | Üs alma $x^y$ | `double, double` | `double` |
| `double fabs(double x)` | Mutlak değer (Ondalıklı) | `double` | `double` |
| `double ceil(double x)` | Yukarı yuvarla (3.1 -> 4.0) | `double` | `double` |
| `double floor(double x)` | Aşağı yuvarla (3.9 -> 3.0) | `double` | `double` |
| `double fmod(double x, double y)` | Ondalıklı mod (Kalan) | `double, double` | `double` |

---

## 2. Trigonometri (Radyan Cinsinden)

| Fonksiyon İmzası | Açıklama | Not |
|------------------|----------|-----|
| `double sin(double x)` | Sinüs | `x` radyan olmalı. |
| `double cos(double x)` | Kosinüs | `x` radyan olmalı. |
| `double tan(double x)` | Tanjant | `x` radyan olmalı. |
| `double asin(double x)` | Ark Sinüs | Sonuç radyan döner. |
| `double atan2(double y, double x)` | Ark Tanjant (y/x) | Koordinat sistemleri için ideal. |

> **Kritik:** Dereceyi radyana çevirmek için: `radyan = derece * (M_PI / 180.0)`.

---

## 3. Logaritma ve Üstel

| Fonksiyon İmzası | Açıklama | Matematiksel Karşılığı |
|------------------|----------|------------------------|
| `double log(double x)` | Doğal Logaritma | $\ln x$ (e tabanında) |
| `double log10(double x)` | 10 Tabanında Logaritma | $\log_{10} x$ |
| `double exp(double x)` | Üstel Fonksiyon | $e^x$ |

---

## 4. Önemli: Derleme Hatası (-lm)

Linux/macOS sistemlerinde `math.h` kullanıyorsanız, derleme komutuna mutlaka `-lm` (Link Math) bayrağını eklemelisiniz.
```bash
gcc program.c -o program -lm
```

---

## 5. Öğrenme Köşesi: NaN ve INFINITY

- **NaN (Not a Number):** Tanımsız sonuç (Örn: `log(-1.0)` veya `sqrt(-1.0)`).
- **INFINITY:** Sonsuz (Örn: `1.0 / 0.0`).

Kontrol fonksiyonları (`math.h` içindedir):
- `int isnan(x)`: Eğer x NaN ise 1 döner.
- `int isinf(x)`: Eğer x sonsuz ise 1 döner.

---

## 6. StackOverflow Notları: `abs` vs `fabs`

En çok yapılan hata: `float` veya `double` bir sayının mutlak değerini almak için `abs()` kullanmak.
- `int abs(int)`: `<stdlib.h>` içindedir, tam sayılar içindir. Ondalık kısmı atar!
- `double fabs(double)`: `<math.h>` içindedir, ondalıklı sayılar içindir.

---

*Hata yapmaktan korkmayın ama bunları bilin:* [09-ek-kaynaklar/yaygin-hatalar-ve-cozumler.md](../09-ek-kaynaklar/yaygin-hatalar-ve-cozumler.md)
