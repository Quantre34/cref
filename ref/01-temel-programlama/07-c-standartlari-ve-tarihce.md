---
baslik: "C Standartları ve Tarihçe (C89'dan C23'e)"
kategori: "meta"
etiketler: ["ansi-c", "c99", "c11", "c23", "standards", "history"]
zorluk: "baslangic"
ilgili: ["01-program-yapisi.md"]
---

# C Standartları ve Tarihçe (C89'dan C23'e)

C dili statik bir dil değildir; zamanla gelişmiş ve yeni özellikler kazanmıştır. Hangi standardı kullandığınızı bilmek, kodunuzun hangi bilgisayarlarda derlenebileceğini belirler.

## 1. K&R C (1972 - 1988)
Brian Kernighan ve Dennis Ritchie'nin yazdığı orijinal C. Fonksiyon prototipleri yoktu, tipler çok esnekti.

## 2. C89 / C90 (ANSI C)
İlk resmi standart.
- `void`, `enum` ve `struct` kuralları netleşti.
- Standart Kütüphane (libc) ilk kez tanımlandı.
- **Derleme:** `gcc -std=c89`

## 3. C99 (Büyük Devrim)
Modern C'nin temeli.
- `//` tek satır yorum geldi.
- Değişkenleri kodun ortasında tanımlama izni.
- `inline`, `restrict`, `long long`, `bool`, `complex` eklendi.
- **VLA** (Variable Length Arrays) tanıtıldı.
- **Derleme:** `gcc -std=c99`

## 4. C11 (Güvenlik ve Performans)
- **Atomics** (`stdatomic.h`) ve **Threads** (`threads.h`) eklendi.
- `_Generic` anahtar kelimesi (Tip-bağımsız makrolar).
- Statik assertionlar (`_Static_assert`).
- **Derleme:** `gcc -std=c11`

## 5. C17 / C18
C11'deki hataların düzeltildiği, yeni özellik eklenmeyen bir "bakım" sürümüdür.
- **Derleme:** `gcc -std=c17`

## 6. C23 (Gelecek ve Modernizasyon)
C'nin en büyük güncellemelerinden biri.
- `constexpr` eklendi.
- `nullptr` (nihayet!) eklendi.
- `typeof` operatörü.
- Fonksiyon parametrelerinde isim vermeden `void` zorunluluğunun kalkması.
- `bool`, `true`, `false` artık anahtar kelime (header gerektirmez).
- **Derleme:** `gcc -std=c23`

---

## Öğrenme Köşesi: Neden Hala C89 Kullanılıyor?
Gömülü sistemlerde (mikrodenetleyiciler) ve çok eski sistemlerde derleyiciler sadece C89 destekleyebilir. Bu yüzden profesyonel kütüphaneler (örn: SQLite) maksimum uyumluluk için hala C89 kurallarına yakın kod yazarlar.

---

*Geleceğin C özelliklerini öğren:* [07-ileri-seviye-c/07-modern-c23-yenilikleri.md](../07-ileri-seviye-c/07-modern-c23-yenilikleri.md)
