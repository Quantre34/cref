---
baslik: "locale.h - Yerel Ayarlar ve Dil Desteği"
kategori: "kutuphane"
etiketler: ["locale", "setlocale", "unicode", "currency", "format"]
zorluk: "orta"
ilgili: ["08-standart-kutuphane-referansi/ctype-detay.md"]
---

# locale.h - Yerel Ayarlar ve Dil Desteği

C programları varsayılan olarak "C" (veya POSIX) yerel ayarıyla başlar. Bu ayarda Türkçe karakterler veya özel para birimi simgeleri tanınmaz. `locale.h` programınızı belirli bir kültüre veya dile göre uyarlamanızı sağlar.

## 1. setlocale()

Programın yerel ayarını değiştirmek için kullanılır.

```c
#include <locale.h>

setlocale(LC_ALL, "tr_TR.UTF-8"); // Her şeyi Türkçe (UTF-8) yap.
```

### Kategoriler:
- `LC_TIME`: Tarih ve saat formatı.
- `LC_MONETARY`: Para birimi simgeleri (Örn: ₺).
- `LC_NUMERIC`: Ondalık ayırıcı (Virgül mü nokta mı?).
- `LC_CTYPE`: Karakter sınıflandırma (Büyük/küçük harf dönüşümü).

---

## 2. lconv Yapısı

Sistemin para birimi ve sayı formatı bilgilerini almak için kullanılır.

```c
struct lconv *lc = localeconv();
printf("Para Birimi: %s\n", lc->currency_symbol);
printf("Ondalık Ayırıcı: %s\n", lc->decimal_point);
```

---

## 3. Öğrenme Köşesi: Unicode ve UTF-8

C programlarında terminalde Türkçe karakterleri (ğ, ü, ş vb.) doğru görmek için işletim sisteminizin, terminalinizin ve programınızın (`setlocale`) aynı UTF-8 ayarında olması gerekir.

---

## 4. StackOverflow Notları: "Empty String" Hilesi

`setlocale(LC_ALL, "");` yazarsanız, C otomatik olarak kullanıcının işletim sistemindeki varsayılan ayarını (Environment variables) okur ve uygular. Taşınabilir kodlar için bu yöntem önerilir.

---

## 5. Gerçek Hayat: Finansal Yazılımlar

Bankacılık gibi alanlarda kullanılan yazılımlar, paranın "1.000,50" mi yoksa "1,000.50" mi yazılacağını `locale.h` üzerinden kontrol ederler.

---

*Dosya sisteminde güvenlik:* [13-guvenlik-ve-kriptografi-temelleri/01-temel-sifreleme.md](../13-guvenlik-ve-kriptografi-temelleri/01-temel-sifreleme.md)
