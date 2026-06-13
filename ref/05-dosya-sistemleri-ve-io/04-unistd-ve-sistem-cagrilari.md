---
baslik: "unistd.h - POSIX Sistem Çağrıları"
kategori: "io"
etiketler: ["unistd", "read", "write", "fork", "pipe", "exec"]
zorluk: "ileri"
ilgili: ["05-dosya-sistemleri-ve-io/02-dosya-okuma-yazma-teknikleri.md"]
---

# unistd.h - POSIX Sistem Çağrıları

`stdio.h` kütüphanesi "yüksek seviyeli" ve taşınabilir fonksiyonlar sunarken, `unistd.h` işletim sisteminin (kernel) kalbine inen "düşük seviyeli" sistem çağrılarını sunar.

## 1. Düşük Seviyeli I/O

Bu fonksiyonlar `FILE*` yerine "File Descriptor" (genellikle bir `int`) kullanır. Tamponlama (buffering) yapmazlar, doğrudan diske/sisteme yazarlar.

- `read(fd, buf, count)`: Dosyadan veri oku.
- `write(fd, buf, count)`: Dosyaya veri yaz.
- `close(fd)`: Dosya tanımlayıcıyı kapat.

---

## 2. Süreç Yönetimi (Process Management)

### `fork()`
Mevcut programın bellekte birebir kopyasını oluşturur.
- **Parent:** Orijinal program.
- **Child:** Kopya program.
Bu fonksiyon, C'de paralel programlamanın en eski yoludur.

### `exec()` Ailesi
Çalışan süreci durdurur ve yerine başka bir programı (Örn: `/bin/ls`) yükleyip çalıştırır.

---

## 3. Diğer Yararlı Fonksiyonlar

- `sleep(seconds)`: Programı belirli saniye kadar duraklatır.
- `usleep(microseconds)`: Mikro saniye seviyesinde duraklatır.
- `getpid()`: Mevcut sürecin kimlik numarasını (PID) verir.
- `access(path, mode)`: Dosyanın var olup olmadığını veya yetkileri kontrol eder.

---

## 4. Öğrenme Köşesi: "Everything is a File"

Unix felsefesine göre her şey (dosyalar, klavye, ekran, ağ bağlantıları) bir dosyadır. `unistd.h` fonksiyonları ile hem bir `.txt` dosyasına, hem de internet üzerinden bir sokete aynı `write` komutuyla veri gönderebilirsiniz.

---

## 5. StackOverflow Notları: "Windows Uyumluluğu"

`unistd.h` bir POSIX standartıdır. Linux ve macOS'ta çalışır ama Windows (MSVC) üzerinde yoktur. Windows'ta benzer işler için `io.h` veya `process.h` kullanılır.

---

## 6. Gerçek Hayat: Kendi Shell'ini Yapmak

Bir terminal (Shell) yapmak istiyorsanız `unistd.h` sizin en iyi dostunuzdur. Kullanıcının yazdığı komutu `fork` ile yeni bir sürece böler, sonra `exec` ile o komutu çalıştırırsınız.

---

*Dosya yetkileri ve detayları:* [sys-stat-detay.md](sys-stat-detay.md)
