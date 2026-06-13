---
baslik: "signal.h - Sinyal Yönetimi"
kategori: "kutuphane"
etiketler: ["signal", "interrupt", "ctrl-c", "segfault", "handler"]
zorluk: "ileri"
ilgili: ["02-bellek-ve-veri-yonetimi/05-bellek-hatalari-ve-guvenlik.md", "07-ileri-seviye-c/04-hata-yonetimi-setjmp-longjmp.md"]
---

# signal.h - Sinyal Yönetimi

Sinyaller, işletim sisteminin çalışan bir programa gönderdiği "acil durum" mesajlarıdır. Örneğin klavyeden `Ctrl+C`'ye bastığınızda işletim sistemi programınıza bir `SIGINT` sinyali gönderir.

## 1. Yaygın Sinyaller

| Sinyal | Anlamı | Neden Olur? |
|--------|--------|-------------|
| `SIGINT` | Interrupt | Kullanıcı `Ctrl+C` bastı. |
| `SIGSEGV` | Segfault | Geçersiz bellek erişimi. |
| `SIGFPE` | Floating Point Error | Sıfıra bölme gibi matematiksel hata. |
| `SIGTERM` | Terminate | Programın düzgünce kapanması isteniyor. |
| `SIGABRT` | Abort | Program `abort()` fonksiyonunu çağırdı (feci hata). |

---

## 2. Sinyalleri Yakalamak (Handling)

Bir sinyal geldiğinde programın ne yapacağını belirleyebilirsiniz.

```c
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void kapaniyor(int sig) {
    printf("\nProgram Ctrl+C ile kapatılıyor. Temizlik yapılıyor...\n");
    exit(0);
}

int main() {
    signal(SIGINT, kapaniyor); // SIGINT gelince kapaniyor() fonksiyonunu çalıştır.
    while(1); // Sonsuz döngü
}
```

---

## 3. Sinyal Göndermek (`raise` ve `kill`)

- `raise(SIGINT)`: Programın kendi kendine sinyal göndermesi.
- `kill(pid, SIGINT)`: Başka bir programa (Process ID üzerinden) sinyal göndermek (Linux/POSIX).

---

## 4. Öğrenme Köşesi: "Async-Signal-Safe" Fonksiyonlar

Sinyal yakalayıcı fonksiyonun (`handler`) içinde her şeyi yapamazsınız! Örneğin `printf` veya `malloc` kullanamazsınız; çünkü sinyal geldiğinde ana program tam o sırada `printf` yapıyor olabilir ve bu durum kilitlenmeye (deadlock) yol açar.
- **Güvenli Fonksiyonlar:** `write`, `_exit`, `signal`.

---

## 5. StackOverflow Notları: `SIG_IGN` ve `SIG_DFL`

- `signal(SIGINT, SIG_IGN)`: Sinyali görmezden gel (Ignore). Artık kullanıcı `Ctrl+C` ile programı kapatamaz.
- `signal(SIGINT, SIG_DFL)`: Standart (Default) davranışa geri dön.

---

## 6. Gerçek Hayat: Graceful Shutdown

Profesyonel sunucu yazılımları `SIGTERM` sinyalini yakalar. Kapanmadan önce açık olan veritabanı bağlantılarını kapatır, dosyaları kaydeder ve "temiz" bir şekilde çıkar. Buna **Graceful Shutdown** denir.

---

*Hata durumunda programı kurtarmak:* [07-ileri-seviye-c/04-hata-yonetimi-setjmp-longjmp.md](../07-ileri-seviye-c/04-hata-yonetimi-setjmp-longjmp.md)
