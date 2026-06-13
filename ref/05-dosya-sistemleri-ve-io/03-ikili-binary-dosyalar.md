---
baslik: "İkili (Binary) Dosyalar"
kategori: "io"
etiketler: ["fread", "fwrite", "fseek", "binary", "rb", "wb"]
zorluk: "ileri"
ilgili: ["02-dosya-okuma-yazma-teknikleri.md", "01-struct-ve-bellek-hizalama.md"]
---

# İkili (Binary) Dosyalar

Metin dosyaları (`.txt`) insanlar içindir. Bilgisayarlar için veriyi olduğu gibi (RAM'deki haliyle) dosyaya kaydetmek çok daha hızlı ve verimlidir. Buna **Binary** depolama denir.

## 1. Neden Binary?

1. **Hız:** Sayıları metne (`42` -> `'4'`, `'2'`) dönüştürmekle uğraşmaz. RAM'deki byte'ları doğrudan diske kopyalar.
2. **Boyut:** Bir `int` her zaman 4 byte kaplar. Metin olarak `12345678` yazarsanız 8 byte kaplar.
3. **Kesinlik:** Ondalıklı sayılarda (`float`) yuvarlama hatası olmaz.

---

## 2. fread ve fwrite

Bu fonksiyonlar bellek bloklarını kopyalar.

```c
struct Oyuncu p = {"Ahmet", 100};
FILE *f = fopen("save.dat", "wb"); // "wb": Write Binary
fwrite(&p, sizeof(struct Oyuncu), 1, f);
fclose(f);
```

---

## 3. fseek ve ftell (Rastgele Erişim)

Bir metin dosyasında 100. satıra gitmek için önceki 99 satırı okumanız gerekir. Binary dosyada ise doğrudan istediğiniz byte'a zıplayabilirsiniz.

- **fseek:** Dosya imlecini istediğin yere taşı.
- **ftell:** Şu an kaçıncı byte'ta olduğunu söyle.
- **rewind:** Dosyanın başına dön.

```c
fseek(f, 5 * sizeof(struct Oyuncu), SEEK_SET); // 6. oyuncuya zıpla.
```

---

## 4. Öğrenme Köşesi: Dosya Boyutu Bulma

Dosyanın sonuna zıplayıp `ftell` ile kaçıncı byte'ta olduğumuzu sorarak dosya boyutunu bulabiliriz:
```c
fseek(f, 0, SEEK_END);
long boyut = ftell(f);
```

---

## 5. StackOverflow Notları: "Endianness" Tehlikesi

Bir bilgisayarda (Little Endian) yazdığınız binary dosyayı, başka bir mimarideki (Big Endian) bilgisayarda okumaya çalışırsanız sayılar saçma sapan çıkar. Binary dosyalar mimariye bağımlıdır.
- **Çözüm:** Ağ üzerinden veri gönderirken veya taşınabilir dosya formatı (PNG, JPG gibi) yaparken veriyi standart bir sıraya sokmalısınız.

---

## 6. Gerçek Hayat: Oyun Kayıtları ve Veritabanları

Modern oyunlar, devasa dünyalarını ve binlerce eşyayı binary dosyalarda saklar. Eğer metin dosyası kullansalardı, yükleme ekranları dakikalarca sürerdi. SQLite gibi veritabanları da özünde çok gelişmiş birer binary dosya yöneticisidir.

---

*Derleyicinin gizli güçleri:* [06-onislemci-ve-derleme-sirlari/01-makrolar-ve-tehlikeleri.md](../06-onislemci-ve-derleme-sirlari/01-makrolar-ve-tehlikeleri.md)
