---
baslik: "time.h - Zaman ve Tarih İşlemleri"
kategori: "kutuphane"
etiketler: ["time", "clock", "epoch", "struct-tm", "timestamp"]
zorluk: "orta"
ilgili: ["08-standart-kutuphane-referansi/stdio-detay.md"]
---

# time.h - Zaman ve Tarih İşlemleri

C'de zaman, genellikle "1 Ocak 1970"ten (Unix Epoch) bu yana geçen saniye sayısı olarak tutulur. `time.h` bu sayıyı anlamlı tarihlere çevirmemizi ve programın ne kadar sürede çalıştığını ölçmemizi sağlar.

## 1. Temel Veri Tipleri

- `time_t`: Genellikle bir `long` tam sayıdır. Toplam saniyeyi tutar.
- `clock_t`: İşlemci tıklarını (ticks) tutar. Süre ölçümü için kullanılır.
- `struct tm`: Tarihi parçalarına (gün, ay, yıl, saat) ayıran yapıdır.

```c
struct tm {
    int tm_sec;   // Saniye (0-60)
    int tm_min;   // Dakika (0-59)
    int tm_hour;  // Saat (0-23)
    int tm_mday;  // Ayın günü (1-31)
    int tm_mon;   // Ay (0-11, Ocak = 0!)
    int tm_year;  // 1900'den beri geçen yıl
    // ...
};
```

---

## 2. Temel Fonksiyonlar

### `time_t time(time_t *tloc)`
- **Görevi:** Şu anki zamanı (Epoch saniyesini) verir.

### `struct tm *localtime(const time_t *timer)`
- **Görevi:** Saniye sayısını, yerel saate göre parçalanmış `struct tm` yapısına çevirir.

### `char *ctime(const time_t *timer)`
- **Görevi:** Saniye sayısını doğrudan okunabilir bir metne çevirir: `"Thu Jun 11 14:30:00 2026\n"`

---

## 3. Program Çalışma Süresini Ölçmek

```c
#include <time.h>

clock_t basla = clock();
// ... yapılacak iş ...
clock_t bitis = clock();

double sure = (double)(bitis - basla) / CLOCKS_PER_SEC;
printf("Süre: %f saniye\n", sure);
```

---

## 4. Öğrenme Köşesi: "Yıl 2038" Problemi (Y2K38)

Eski sistemlerde `time_t` 32-bit bir tam sayıdır. 19 Ocak 2038 tarihinde bu sayı kapasitesini aşacak ve tarih bir anda 1901'e geri dönecektir. Modern 64-bit sistemlerde bu sorun çözülmüştür; 64-bit `time_t` evrenin sonuna kadar yeterlidir!

---

## 5. StackOverflow Notları: `tm_year` ve `tm_mon` Şaşırtmacası

- `tm_mon` (Ay) 0'dan başlar. Yani Mart ayı `2`'dir.
- `tm_year` (Yıl) 1900'den farkı tutar. 2024 yılı için `124` yazar.
**Doğru Yazım:** `printf("Yıl: %d", t->tm_year + 1900);`

---

## 6. Gerçek Hayat: Log ve Timestamp

Veritabanlarına veri kaydederken genellikle `time(NULL)` ile alınan ham saniye değeri (Timestamp) kaydedilir. Bu sayede dünyanın her yerindeki kullanıcılar, o sayıyı kendi yerel saatlerine göre kolayca çevirebilirler.

---

*Dosya zamanlarını kontrol etmek:* [sys-stat-detay.md](sys-stat-detay.md)
