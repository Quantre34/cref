---
baslik: "Dosya Okuma ve Yazma Teknikleri"
kategori: "io"
etiketler: ["fopen", "fclose", "fprintf", "fscanf", "fgets", "file"]
zorluk: "orta"
ilgili: ["01-standart-io-akislari.md", "03-ikili-binary-dosyalar.md"]
---

# Dosya Okuma ve Yazma Teknikleri

Dosyalar, programınız kapandığında bile verilerin kalıcı olmasını sağlar. C'de her dosya bir `FILE` yapısı (struct) üzerinden yönetilir.

## 1. Dosya Açma ve Kapatma

Bir dosyayla çalışmak için önce onu açmalı, işiniz bitince de mutlaka kapatmalısınız.

```c
FILE *f = fopen("veri.txt", "w"); // "w": Yazma modu
if (f == NULL) {
    perror("Dosya açılamadı");
    return 1;
}

// İşlemler...

fclose(f); // MUTLAKA kapat!
```

### Dosya Modları:
- `"r"` (Read): Sadece okuma. Dosya yoksa hata verir.
- `"w"` (Write): Yazma. Dosya varsa içeriğini siler, yoksa oluşturur.
- `"a"` (Append): Ekleme. Mevcut verinin sonuna yazar.
- `"r+"`: Hem okuma hem yazma.

---

## 2. Yazma ve Okuma Fonksiyonları

| Fonksiyon | Görevi | Not |
|-----------|--------|-----|
| `fprintf()` | Biçimlendirilmiş yazı yazar. | Tıpkı `printf` gibidir. |
| `fputs()` | Ham string yazar. | Hızlıdır. |
| `fscanf()` | Biçimlendirilmiş veri okur. | Boşlukları geçemez. |
| `fgets()` | Bir satırın tamamını okur. | **En güvenli yoldur.** |

---

## 3. Öğrenme Köşesi: Satır Satır Okuma

Bir dosyayı baştan sona okumanın en sağlam yolu:
```c
char satir[256];
while (fgets(satir, sizeof(satir), f)) {
    printf("%s", satir);
}
```

---

## 4. StackOverflow Notları: "EOF" (End Of File)

`while(!feof(f))` kullanımı yaygın ama **hatalıdır.** `feof` ancak bir okuma denemesi başarısız olduktan sonra "True" döner. Bu yüzden son satırı iki kez okuyabilirsiniz. Bunun yerine `fgets` veya `fscanf`'in dönüş değerini kontrol edin.

---

## 5. Gerçek Hayat: Konfigürasyon Dosyaları

Çoğu oyun ve program, ayarlarını `.txt` veya `.ini` dosyalarında saklar. Program açıldığında bu dosyayı satır satır okur, her satırı parçalar (`strtok` gibi fonksiyonlarla) ve değişkenlerine atar.

---

*Hızlı ve büyük veriler için:* [03-ikili-binary-dosyalar.md](03-ikili-binary-dosyalar.md)
