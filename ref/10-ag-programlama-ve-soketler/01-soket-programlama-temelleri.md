---
baslik: "Soket Programlama Giriş"
kategori: "network"
etiketler: ["socket", "tcp", "udp", "ip", "networking", "sys/socket.h"]
zorluk: "ileri"
ilgili: ["05-dosya-sistemleri-ve-io/04-unistd-ve-sistem-cagrilari.md"]
---

# Soket Programlama Giriş

C ile ağ üzerinden başka bilgisayarlarla konuşmak için **Soketler (Sockets)** kullanılır. Unix felsefesinde soketler de birer dosyadır (`write` ve `read` ile veri gönderilir).

## 1. Temel Kavramlar

- **IP Adresi:** Bilgisayarın adresi (Örn: 192.168.1.1).
- **Port:** Uygulamanın kapı numarası (Örn: HTTP için 80).
- **TCP:** Güvenli, sıralı veri (Dosya transferi için).
- **UDP:** Hızlı, sırasız veri (Canlı yayın/Oyun için).

---

## 2. Temel Fonksiyonlar (`sys/socket.h`)

| Fonksiyon İmzası | Açıklama |
|------------------|----------|
| `int socket(int domain, int type, int protocol)` | Soket oluşturur (dosya tanımlayıcı döner). |
| `int bind(int fd, const struct sockaddr *addr, ...)` | Soketi bir port numarasına bağlar (Sunucu için). |
| `int listen(int fd, int backlog)` | Gelen bağlantıları beklemeye başlar. |
| `int accept(int fd, ...)` | Gelen bağlantıyı kabul eder. |
| `int connect(int fd, ...)` | Bir sunucuya bağlanır (İstemci için). |

---

## 3. Byte Sıralaması (Endianness)

İnternet dünyası "Big Endian" kullanırken, senin bilgisayarın (x86) "Little Endian" kullanabilir. Sayıları ağa göndermeden önce çevirmelisin:
- `htons()`: Host to Network Short (Port için).
- `htonl()`: Host to Network Long (IP için).

---

## 4. Öğrenme Köşesi: İstemci-Sunucu Akışı

1. **Sunucu:** `socket()` -> `bind()` -> `listen()` -> `accept()` -> `read/write`
2. **İstemci:** `socket()` -> `connect()` -> `write/read`

---

## 5. StackOverflow Notları: "Address already in use"

Programı kapatıp hemen açtığında bu hatayı alabilirsin. İşletim sistemi portu bir süre daha meşgul tutar. Bunu aşmak için `setsockopt()` ile `SO_REUSEADDR` bayrağını kullanmalısın.

---

*Detaylı IP işlemleri:* [arpa-inet-detay.md](arpa-inet-detay.md)
