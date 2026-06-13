---
baslik: "ctype.h - Karakter Sınıflandırma"
kategori: "kutuphane"
etiketler: ["ctype", "isdigit", "isalpha", "toupper", "tolower"]
zorluk: "baslangic"
ilgili: ["01-temel-programlama/03-veri-tipleri-derinlemesine.md"]
---

# ctype.h - Karakter Sınıflandırma

Bir karakterin harf mi, rakam mı yoksa boşluk mu olduğunu manuel olarak kontrol etmek (Örn: `if (c >= '0' && c <= '9')`) yorucudur ve hata payı yüksektir. `ctype.h` bu işlemleri hızlı ve güvenli yapan fonksiyonlar sunar.

## 1. Sınıflandırma Fonksiyonları

| Fonksiyon | Karakter şu ise TRUE döner: |
|-----------|-----------------------------|
| `isdigit(c)` | Rakam (0-9) |
| `isalpha(c)` | Harf (A-Z, a-z) |
| `isalnum(c)` | Harf veya Rakam |
| `isspace(c)` | Boşluk, Tab, Yeni Satır (`\n`) |
| `islower(c)` | Küçük harf |
| `isupper(c)` | Büyük harf |
| `ispunct(c)` | Noktalama işareti |

---

## 2. Dönüştürme Fonksiyonları

- `toupper(c)`: Karakteri büyük harfe çevirir.
- `tolower(c)`: Karakteri küçük harfe çevirir.

```c
char c = 'a';
char buyuk = toupper(c); // 'A'
```

---

## 3. Öğrenme Köşesi: "ASCII" Bağımlılığı

Bu fonksiyonlar sadece standart 128 karakterlik ASCII tablosunda değil, sistemin yerel ayarlarına (**Locale**) göre de çalışabilir. Örneğin Türkçe bir yerel ayar kullanıyorsanız, `islower('ğ')` true dönebilir (Eğer derleyici destekliyorsa).

---

## 4. StackOverflow Notları: "int" Dönüş Tipi

Bu fonksiyonlar parametre olarak `int` alır ve dönüş olarak `int` (0 veya 1) verirler. `char` gönderirken aslında ASCII kodunu göndermiş olursunuz.

---

## 5. Gerçek Hayat: Veri Temizleme (Sanitization)

Kullanıcıdan bir şifre veya kullanıcı adı aldığınızda, içinde geçersiz karakterler olup olmadığını `ctype.h` fonksiyonlarıyla bir döngü içinde kontrol etmek güvenlik için standart bir uygulamadır.

---

*Geniş karakterler (Unicode) için:* [wchar-detay.md](wchar-detay.md)
