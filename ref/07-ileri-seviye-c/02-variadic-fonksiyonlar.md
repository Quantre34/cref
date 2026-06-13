---
baslik: "Variadic Fonksiyonlar (Belirsiz Parametre)"
kategori: "ileri"
etiketler: ["va_list", "printf", "stdarg", "ellipses", "variadic"]
zorluk: "ileri"
ilgili: ["01-fonksiyonlar-ve-stack-frame.md", "01-fonksiyon-pointerlari.md"]
---

# Variadic Fonksiyonlar (Belirsiz Parametre)

`printf` fonksiyonunun nasıl olup da bazen 1, bazen 10 parametre alabildiğini merak ettiniz mi? İşte bunu sağlayan teknolojiye **Variadic Functions** denir.

## 1. Sözdizimi: Üç Nokta (...)

Bir fonksiyonun belirsiz sayıda parametre alacağını `...` (ellipses) ile belirtiriz. Ancak en az bir tane "sabit" parametre olmalıdır.

```c
#include <stdarg.h>

double ortalama(int adet, ...) {
    va_list liste;        // 1. Parametre listesini oluştur.
    va_start(liste, adet); // 2. Başlat (adet'ten sonrasını oku).
    
    double toplam = 0;
    for(int i = 0; i < adet; i++) {
        toplam += va_arg(liste, int); // 3. Sıradaki parametreyi int olarak al.
    }
    
    va_end(liste); // 4. Temizle.
    return toplam / adet;
}
```

---

## 2. İşleyiş: Stack Üzerinde Gezinmek

Fonksiyon çağrıldığında tüm parametreler bellekteki **Stack**'e yan yana dizilir. `stdarg.h` içindeki makrolar, aslında bu stack üzerinde adres kaydırarak sıradaki veriyi okumanızı sağlar.

---

## 3. Büyük Tehlike: Tip Güvenliği Yok!

C, variadic parametrelerin tipini çalışma zamanında kontrol edemez.
- `va_arg(liste, int)` derseniz ama kullanıcı `double` gönderdiyse, C o adresteki byte'ları `int` gibi okumaya çalışır ve saçma bir sonuç (veya çökme) üretir.
- **Neden printf format string ister?** Çünkü içindeki `%d`, `%s` gibi belirteçler, `va_arg`'a hangi tipi okuması gerektiğini söyler.

---

## 4. Öğrenme Köşesi: "Sentinels" (Bekçiler)

Kaç parametre geldiğini anlamanın iki yolu vardır:
1. İlk parametrede sayıyı vermek (yukarıdaki `adet` gibi).
2. Son parametreye özel bir "bitiş" değeri koymak (örneğin `NULL`).

---

## 5. StackOverflow Notları: `vprintf` Ailesi

Kendi `printf` fonksiyonunuzu yazmak isterseniz (örneğin loglama için), `printf` yerine `vprintf` kullanmalısınız. Çünkü `va_list` tipindeki bir listeyi ancak `v` ile başlayan fonksiyonlar kabul eder.

---

## 6. Gerçek Hayat: Esnek Fonksiyonlar

Loglama sistemlerinde çok kullanılır:
```c
log_yaz(LOG_HATA, "Kullanıcı %s bulunamadı. Hata kodu: %d", isim, kod);
```
Bu sayede her hata mesajı için ayrı bir fonksiyon yazmak zorunda kalmazsınız.

---

*Hızlı ve yakın çalışma:* [03-inline-ve-register.md](03-inline-ve-register.md)
