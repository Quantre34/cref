---
baslik: "Veri Tipleri Derinlemesine"
kategori: "temel"
etiketler: ["int", "float", "char", "signed", "unsigned", "limits"]
zorluk: "baslangic"
ilgili: ["02-degiskenler-ve-bellek.md", "15-bit-islemleri.md"]
---

# Veri Tipleri Derinlemesine

C'de veri tipleri, derleyiciye belleğin nasıl yorumlanacağını söyler. Aynı `01000001` bit dizisi, bir tipe göre `65` sayısıyken, başka bir tipe göre `'A'` harfidir.

## 1. Tam Sayı Tipleri (Integers)

Tam sayılar bellekte doğrudan ikilik (binary) sistemde saklanır.

| Tip | Boyut (Byte)* | Menzil | Format |
|-----|---------------|--------|--------|
| `char` | 1 | -128 ile 127 | `%c` veya `%d` |
| `int` | 4 | -2 Milyar ile +2 Milyar | `%d` |
| `short` | 2 | -32,768 ile 32,767 | `%hd` |
| `long` | 4 veya 8 | Çok büyük! | `%ld` |
| `long long` | 8 | Devasa! | `%lld` |

*\*Boyutlar sisteme göre değişebilir. Kesin bilgi için `sizeof()` kullanın.*

---

## 2. İşaretli ve İşaretsiz (Signed vs Unsigned)

İşlemciler en soldaki biti (Most Significant Bit - MSB) "işaret biti" olarak kullanabilir.

- **signed (varsayılan):** En soldaki bit `1` ise sayı negatiftir.
- **unsigned:** En soldaki bit de sayıya dahildir. Sadece pozitif sayılar saklanır, ama menzil iki katına çıkar.

```c
unsigned int x = -5; // TEHLİKELİ! Sayı çok büyük bir pozitif sayıya dönüşür.
```

---

## 3. Ondalıklı Sayılar (Floating Point)

Ondalıklı sayılar bellekte **IEEE 754** standardına göre saklanır. Yani bir kısmı sayıyı (mantissa), bir kısmı üstü (exponent) tutar.

| Tip | Boyut | Hassasiyet | Format |
|-----|-------|------------|--------|
| `float` | 4 byte | ~7 basamak | `%f` |
| `double` | 8 byte | ~15 basamak | `%lf` |

### İşleyiş: Neden `0.1 + 0.2 != 0.3`?
Bilgisayarlar bazı ondalıklı sayıları ikilik sistemde sonsuz bir döngü olmadan temsil edemez. Bu yüzden ufak yuvarlama hataları oluşur.
- **Kritik Kural:** Finansal hesaplamalarda asla `float` kullanmayın!

---

## 4. Karakterler (char)

C'de `char` aslında küçük bir tam sayıdır. Her harfin bir **ASCII** kodu vardır.

```c
char harf = 'A';
printf("%d", harf); // 65 yazdırır.
printf("%c", harf + 1); // 'B' yazdırır.
```

---

## 5. Öğrenme Köşesi: Integer Overflow (Taşma)

Bir bardağa kapasitesinden fazla su koyarsanız taşar. Veri tiplerinde de öyledir.

**Deney:**
```c
signed char x = 127;
x = x + 1;
printf("%d", x); // Çıktı: -128!
```
**Neden?** 127 (`01111111`), 1 eklenince `10000000` olur. Signed dünyasında bu `-128` demektir.

---

## 6. StackOverflow Notları: "sizeof" Bir Fonksiyon Değildir

`sizeof(int)` yazdığınızda bu bir fonksiyon çağrısı değildir. Bu bir **operatör**dür ve derleme aşamasında sonucu hesaplanıp yerine sayı yazılır. Program çalışırken `sizeof` için bir işlem yapılmaz.

---

## 7. Gerçek Hayat: stdint.h ve Kesin Boyutlar

`int` her sistemde 4 byte olmayabilir (eski sistemlerde 2 byte'tır). Eğer kesin boyuta ihtiyacınız varsa (örneğin bir dosya formatı yazıyorsanız), `<stdint.h>` kullanın:

- `int32_t`: Her zaman 32 bit (4 byte).
- `uint8_t`: Her zaman 8 bit (1 byte, işaretsiz).

---

*Hangi veri tipiyle hangi işlemleri yapabiliriz?* [04-operatorler-ve-oncelik.md](04-operatorler-ve-oncelik.md)
