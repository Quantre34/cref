---
baslik: "arpa/inet.h - IP Adres Dönüşümleri"
kategori: "network"
etiketler: ["inet_addr", "inet_ntoa", "ip-conversion", "binary"]
zorluk: "ileri"
ilgili: ["01-soket-programlama-temelleri.md"]
---

# arpa/inet.h - IP Adres Dönüşümleri

Bilgisayarlar IP adreslerini metin olarak ("127.0.0.1") değil, 32-bitlik tam sayılar olarak saklar. `arpa/inet.h`, insan dilli adreslerle bilgisayar dilli adresler arasındaki köprüdür.

## 1. Temel Fonksiyonlar

### `in_addr_t inet_addr(const char *cp)`
- **Girdi:** "192.168.1.1" gibi bir string.
- **Çıktı:** Bilgisayarın anlayacağı 32-bit binary sayı.

### `char *inet_ntoa(struct in_addr in)`
- **Görevi:** Network to ASCII. Binary adresi tekrar okunabilir metne çevirir.

---

## 2. Modern Fonksiyonlar (IPv6 Desteği)

Modern projelerde `inet_pton` (Presentation to Network) ve `inet_ntop` kullanılması önerilir çünkü hem IPv4 hem IPv6 desteklerler.

```c
#include <arpa/inet.h>
struct in_addr addr;
inet_pton(AF_INET, "1.1.1.1", &addr); // Metinden Binary'ye
```

---

## 3. Öğrenme Köşesi: "AF_INET" Nedir?

- **AF:** Address Family (Adres Ailesi).
- **INET:** IPv4 protokolü demektir. IPv6 için `AF_INET6` kullanılır.

---

## 4. Gerçek Hayat: Kim Bağlandı?

Bir sunucu yazdığında, `accept()` fonksiyonu sana bağlanan kişinin adresini binary olarak verir. Sen bu kütüphaneyi kullanarak "Ekranıma bağlanan kişinin IP'sini yazdır" diyebilirsin.

---

*Hızlı veri arama teknikleri:* [11-algoritmalar-ve-ileri-veri-yapilari/01-hash-maps.md](../11-algoritmalar-ve-ileri-veri-yapilari/01-hash-maps.md)
