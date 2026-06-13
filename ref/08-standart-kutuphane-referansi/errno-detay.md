---
baslik: "errno.h - Hata Raporlama"
kategori: "kutuphane"
etiketler: ["errno", "perror", "strerror", "hata-ayiklama"]
zorluk: "baslangic"
ilgili: ["09-ek-kaynaklar/yaygin-hatalar-ve-cozumler.md"]
---

# errno.h - Hata Raporlama

C kütüphane fonksiyonlarının çoğu, bir hata oluştuğunda "neden hata olduğunu" söylemek için `errno` denilen global bir değişkeni kullanır.

## 1. errno Nedir?

`errno`, sistem genelinde bir tam sayıdır. Bir fonksiyon başarısız olduğunda, hatanın türüne göre bu sayıya bir değer atanır.

### Yaygın Hata Kodları:
- `EDOM`: Matematiksel alan hatası (Örn: `sqrt(-1)`).
- `ERANGE`: Sonuç çok büyük/küçük (Menzil hatası).
- `EACCES`: Erişim engellendi (İzin hatası).
- `ENOENT`: Dosya veya dizin bulunamadı.

---

## 2. Hata Mesajlarını Yazdırmak

Ham sayıları (Örn: `errno = 2`) görmek kullanıcı için bir şey ifade etmez. İki temel yardımcımız vardır:

### `void perror(const char *s)`
- **Görevi:** Kendi mesajınızın yanına, `errno`'nun o anki karşılığını metin olarak basar.
```c
FILE *f = fopen("yok.txt", "r");
if (!f) perror("Dosya Hatası"); 
// Çıktı: Dosya Hatası: No such file or directory
```

### `char *strerror(int errnum)`
- **Görevi:** Hata kodunun metin karşılığını döndürür.
```c
printf("Hata: %s", strerror(errno));
```

---

## 3. Kritik Kural: errno'yu Hemen Kontrol Edin!

`errno` sadece bir hata olduğunda güncellenir. Başarılı olan fonksiyonlar `errno`'yu **temizlemez**.
```c
FILE *f = fopen("yok.txt", "r"); // Hata oldu, errno = ENOENT
// ... araya başka kodlar girdi ...
if (errno == ENOENT) { ... } // TEHLİKELİ! Başka bir şey errno'yu değiştirmiş olabilir.
```
**Doğru Yaklaşım:** Hata olduğu anda `errno` değerini yerel bir değişkene kopyalayın veya hemen yazdırın.

---

## 4. Öğrenme Köşesi: errno'yu Sıfırlamak

Bazı durumlarda fonksiyonu çağırmadan önce `errno = 0;` yaparak temiz bir başlangıç yapmak en sağlam yoldur.

---

## 5. StackOverflow Notları: "Thread-Safety"

Eskiden `errno` tek bir global değişkendi. Ancak çok kanallı (multi-thread) programlarda bu bir faciadır (Thread A hata yapınca Thread B'nin `errno`'su değişir). Modern sistemlerde her thread'in kendine özel (`thread-local`) bir `errno` alanı vardır.

---

*Hata durumunda büyük zıplama:* [07-ileri-seviye-c/04-hata-yonetimi-setjmp-longjmp.md](../07-ileri-seviye-c/04-hata-yonetimi-setjmp-longjmp.md)
