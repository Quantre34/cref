---
baslik: "Standart I/O Akışları (Streams)"
kategori: "io"
etiketler: ["stdin", "stdout", "stderr", "stream", "buffer", "akisyapisi"]
zorluk: "baslangic"
ilgili: ["01-program-yapisi.md", "02-dosya-okuma-yazma-teknikleri.md"]
---

# Standart I/O Akışları (Streams)

C'de giriş ve çıkış işlemleri "akışlar" (streams) üzerinden yürür. Programınız çalıştığı anda işletim sistemi ona üç tane hazır kanal bağlar.

## 1. Üç Temel Akış

1. **stdin (Standard Input):** Standart giriş. Genellikle klavyedir.
2. **stdout (Standard Output):** Standart çıkış. Genellikle ekran (terminal).
3. **stderr (Standard Error):** Standart hata. Hata mesajları için özel kanal.

---

## 2. Neden stderr Var?

`stdout` ve `stderr` ikisi de ekrana yazı yazdırır. Ama aralarında hayati bir fark vardır: **Buffering (Tamponlama)**.

- **stdout tamponludur:** Yazdığınız şey hemen ekrana gitmeyebilir. C, performans için bir miktar yazıyı biriktirip topluca basar.
- **stderr tamponsuzdur:** Yazdığınız anda ekrana basılır. Eğer program çökerse, son hata mesajını görmenizi sağlar.

**Altın Kural:** Hata mesajlarını `printf` ile değil, `fprintf(stderr, ...)` ile yazdırın.

---

## 3. Redirection (Yönlendirme)

Terminalden bu akışların nereye gideceğini değiştirebilirsiniz:

```bash
./program > cikis.txt    # stdout'u dosyaya yaz.
./program 2> hatalar.txt # stderr'i dosyaya yaz.
./program < girdi.txt    # Klavyeden değil, dosyadan oku.
```

---

## 4. Öğrenme Köşesi: `fflush()` Nedir?

Bazen `stdout` tamponunun dolmasını beklemeden yazının ekrana gitmesini istersiniz.
```c
printf("Lütfen bekleyin...");
fflush(stdout); // Yazıyı hemen ekrana "iteler".
```

---

## 5. StackOverflow Notları: "Newline" Etkisi

Çoğu sistemde `\n` karakteri `stdout`'u otomatik olarak temizler (flush). Eğer satır sonu koymazsanız, yazınız program bitene kadar ekranda görünmeyebilir.

---

## 6. Gerçek Hayat: Log Dosyaları

Profesyonel sunucu yazılımları, normal çıktıları bir log dosyasına, kritik hataları ise başka bir log dosyasına (veya bir uyarı sistemine) yönlendirir. Bu ayrımı `stdout` ve `stderr` kullanarak zahmetsizce yaparlar.

---

*Dosyaları açmak ve kapatmak:* [02-dosya-okuma-yazma-teknikleri.md](02-dosya-okuma-yazma-teknikleri.md)
