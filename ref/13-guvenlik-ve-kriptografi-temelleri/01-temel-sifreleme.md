---
baslik: "Temel Şifreleme ve Hash (Security)"
kategori: "guvenlik"
etiketler: ["encryption", "hashing", "security", "sha256", "aes"]
zorluk: "ileri"
ilgili: ["11-algoritmalar-ve-ileri-veri-yapilari/01-hash-maps.md"]
---

# Temel Şifreleme ve Hash (Security)

C ile güvenli yazılım geliştirmek için verilerin nasıl saklanacağını ve korunacağını bilmek gerekir. C standart kütüphanesinde gelişmiş şifreleme yoktur; ancak OpenSSL gibi kütüphanelerle bu işlemler yapılır.

## 1. Hashing (Özetleme)

Hashing, veriyi geri döndürülemez bir parmak izine dönüştürür. Şifreler asla veritabanında açık metin olarak saklanmaz.

- **MD5:** Artık güvensiz (Hızlı ama kırılabilir).
- **SHA-256:** Şu anki standart.

```c
// Analoji: Bir yemeğin tadı (Hash), malzemelere (Veri) bağlıdır. 
// Tattan yola çıkarak yemeği geri oluşturamazsınız (Geri döndürülemez).
```

---

## 2. Simetrik Şifreleme (AES)

Aynı anahtar ile hem şifreleme hem de şifre çözme yapılır.

**Analoji:** Tek bir anahtarı olan bir asma kilit. Kilitleyen de açan da aynı anahtarı kullanır.

---

## 3. Asimetrik Şifreleme (RSA)

İki farklı anahtar vardır: **Public Key** (Herkes bilir) ve **Private Key** (Sadece sende).
- Public Key ile şifrelenen bir veriyi sadece Private Key sahibi açabilir.

---

## 4. Öğrenme Köşesi: Salt (Tuzlama)

Aynı şifreyi kullanan iki kişinin hash değerleri aynı olur. Bunu önlemek için şifrenin sonuna rastgele bir metin (Salt) eklenir:
`Hash(Sifre + Salt)`

---

## 5. StackOverflow Notları: "Don't Roll Your Own Crypto"

Asla kendi şifreleme algoritmanı yazma! Şifreleme çok karmaşık bir matematiksel süreçtir ve senin fark etmediğin küçük bir açık, tüm verilerin çalınmasına yol açar. Her zaman OpenSSL veya Libsodium gibi test edilmiş kütüphaneleri kullan.

---

## 6. Gerçek Hayat: HTTPS

İnternette gezdiğiniz her web sitesi (HTTPS), arka planda C ile yazılmış şifreleme kütüphanelerini kullanarak verilerini korur.

---

*Hata ayıklama ve bellek sızıntıları:* [09-ek-kaynaklar/gdb-ve-valgrind-rehberi.md](../09-ek-kaynaklar/gdb-ve-valgrind-rehberi.md)
