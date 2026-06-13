---
baslik: "stdint.h ve limits.h - Tip Güvenliği ve Sınırlar"
kategori: "kutuphane"
etiketler: ["stdint", "limits", "int32_t", "INT_MAX", "tasinabilirlik"]
zorluk: "baslangic"
ilgili: ["01-temel-programlama/03-veri-tipleri-derinlemesine.md"]
---

# stdint.h ve limits.h - Tip Güvenliği ve Sınırlar

C'de `int` veya `long` gibi tiplerin boyutları işlemciden işlemciye değişebilir. Bu durum, yazdığınız kodun başka bir bilgisayarda hatalı çalışmasına (taşmalara) yol açabilir. Bu iki kütüphane, "kesin boyutlu" tipler ve "menzil sınırları" sunarak kodunuzu taşınabilir hale getirir.

## 1. Kesin Boyutlu Tipler (`stdint.h`)

Bu tipleri kullandığınızda, verinin kaç bit kapladığı her sistemde aynı kalır.

| Tip | Boyut (Bit) | Boyut (Byte) | Menzil |
|-----|-------------|--------------|--------|
| `int8_t` | 8 | 1 | -128 to 127 |
| `uint8_t` | 8 | 1 | 0 to 255 |
| `int16_t` | 16 | 2 | -32,768 to 32,767 |
| `int32_t` | 32 | 4 | ~2 Milyar |
| `uint32_t` | 32 | 4 | ~4 Milyar |
| `int64_t` | 64 | 8 | Devasa! |

**Neden Kullanmalı?** Eğer bir dosya formatı (örn: PNG) veya ağ paketi tasarlıyorsanız, `int`'in 2 mi 4 mü byte olduğunu tahmin etmek yerine `int32_t` kullanarak hatayı sıfıra indirirsiniz.

---

## 2. Tip Sınırları (`limits.h`)

Bir değişkenin içine en fazla kaç yazabileceğinizi öğrenmek için bu sabitleri kullanırsınız.

- `INT_MAX`: Bir `int`'in alabileceği en büyük değer.
- `INT_MIN`: Bir `int`'in alabileceği en küçük değer.
- `CHAR_BIT`: Bir byte içindeki bit sayısı (Genelde 8).
- `UINT_MAX`: `unsigned int` sınırı.

```c
#include <limits.h>
if (sayi > INT_MAX - 100) {
    printf("Dikkat! Taşma (Overflow) yaklaşıyor.");
}
```

---

## 3. float.h (Ondalıklı Sınırlar)

Aynı mantık ondalıklı sayılar için `float.h` içinde geçerlidir:
- `FLT_DIG`: `float`'ın güvenilir ondalık basamak sayısı (Genelde 6).
- `DBL_MAX`: `double`'ın tavan değeri.

---

## 4. Öğrenme Köşesi: "Size-independent" Kod

Sisteminizin 32-bit mi 64-bit mi olduğunu bilmediğiniz durumlarda, bir pointer'ı tam sayıya çevirmeniz gerekirse `intptr_t` kullanın. Bu tip, o sistemdeki pointer boyutuyla otomatik olarak eşleşir.

---

## 5. StackOverflow Notları: `%d` ile `int32_t` Basmak

`int32_t` gibi tipleri `printf` ile basarken bazen uyarı alabilirsiniz. En doğru yol `<inttypes.h>` kütüphanesini dahil edip `PRI` makrolarını kullanmaktır:
```c
#include <inttypes.h>
int32_t x = 500;
printf("Değer: %" PRId32 "\n", x);
```

---

## 6. Gerçek Hayat: Kriptografi ve Güvenlik

Şifreleme algoritmaları (AES, SHA-256 vb.) bit seviyesinde çok hassas işlemler yapar. Bu algoritmaların kaynak kodlarına bakarsanız, neredeyse hiç `int` görmezsiniz; her yer `uint32_t` ve `uint64_t` ile doludur.

---

*Hangi tip ne kadar yer kaplar?* [01-temel-programlama/03-veri-tipleri-derinlemesine.md](../01-temel-programlama/03-veri-tipleri-derinlemesine.md)
