---
baslik: "GDB ve Valgrind Rehberi"
kategori: "debug"
etiketler: ["gdb", "valgrind", "debug", "hata-ayiklama", "bellek-sizintisi"]
zorluk: "orta"
ilgili: ["00-kurulum-ve-hazirlik.md", "02-bellek-ve-veri-yonetimi/04-dinamik-bellek-yonetimi.md"]
---

# GDB ve Valgrind Rehberi

Programınız çalışıyor ama yanlış sonuç veriyor veya aniden çöküyorsa, gözlerinizle göremeyeceğiniz hataları bulmak için bu iki aracı kullanmalısınız.

## 1. GDB (GNU Debugger)

GDB, programınızı satır satır çalıştırmanızı ve o anda değişkenlerin içinde ne olduğunu görmenizi sağlar.

### Hazırlık:
Derlerken mutlaka `-g` bayrağını ekleyin:
```bash
gcc -g program.c -o program
```

### Temel Komutlar:
1. `gdb ./program`: Başlat.
2. `break main`: main fonksiyonunda dur.
3. `run` (r): Programı çalıştır.
4. `next` (n): Sonraki satıra geç (Fonksiyonun içine girmez).
5. `step` (s): Sonraki satıra geç (Fonksiyonun içine girer).
6. `print x` (p x): x değişkeninin değerini göster.
7. `backtrace` (bt): Program çöktüğünde "Hangi fonksiyonun içinden hangi fonksiyon çağrıldı da buraya gelindi?" sorusunun cevabını (Call Stack) verir.
8. `quit` (q): Çık.

---

## 2. Valgrind (Bellek Dedektifi)

Valgrind, programınızdaki bellek sızıntılarını (leak) ve geçersiz bellek erişimlerini bulur.

### Kullanım:
```bash
valgrind --leak-check=full ./program
```

### Rapor Okuma:
- **"definitely lost":** `free` etmeyi unuttuğunuz kesin olan bellek.
- **"invalid read/write":** Dizinin dışına çıktığınızda veya `free` edilmiş bir yeri okumaya çalıştığınızda bu mesajı görürsünüz.

---

## 3. Öğrenme Köşesi: "Core Dump" Nedir?

Programınız çöktüğünde işletim sistemi bazen o anki RAM'in bir kopyasını (`core` dosyası) diske yazar. GDB ile bu dosyayı açıp programın tam olarak neden ve hangi satırda öldüğünü "otopsi" yaparmış gibi inceleyebilirsiniz.

---

## 4. StackOverflow Notları: "GDB vs Printf"

Birçok yazılımcı hata ayıklamak için kodun her yerine `printf` koyar. Buna "Printf Debugging" denir. Küçük işlerde işe yarasa da, büyük ve karmaşık projelerde GDB kullanmak çok daha profesyonel ve hızlı bir yöntemdir.

---

*Gerçek dünya projeleri:* [real-world-c-projects.md](real-world-c-projects.md)
