---
baslik: "Inline ve Register (Performans)"
kategori: "ileri"
etiketler: ["inline", "register", "optimizasyon", "hiz", "derleyici"]
zorluk: "orta"
ilgili: ["01-makrolar-ve-tehlikeleri.md", "18-derleme-ve-debug.md"]
---

# Inline ve Register (Performans)

Derleyiciye kodunuzu nasıl optimize etmesi gerektiği konusunda ipuçları verebilirsiniz. `inline` ve `register` bu amaçla kullanılan iki eski ama önemli anahtar kelimedir.

## 1. inline: Fonksiyonu "Yapıştır"

Bir fonksiyon çok küçükse ve çok sık çağrılıyorsa (örneğin bir döngü içinde milyonlarca kez), fonksiyon çağırma maliyeti (stack frame hazırlığı, zıplama vb.) asıl işten daha uzun sürebilir.

```c
inline int topla(int a, int b) {
    return a + b;
}
```
`inline` derleyiciye şunu söyler: "Bu fonksiyonu çağırmak için zahmet etme, kodunu doğrudan çağrıldığı yere kopyala-yapıştır."

### Önemli Not:
`inline` bir **emir** değil, bir **tavsiye**dir. Derleyici fonksiyonun çok büyük olduğunu veya inline yapmanın zararlı olacağını düşünürse tavsiyenizi reddedebilir.

---

## 2. register: Değişkeni İşlemciye Koy

Bilgisayarda en hızlı yer RAM değil, işlemcinin (CPU) içindeki **Register** (Yazmaç) alanlarıdır.

```c
register int i;
for (i = 0; i < 1000000; i++) { ... }
```
`register` derleyiciye şunu söyler: "Bu değişken çok sık kullanılıyor, onu RAM'de saklamak yerine mümkünse işlemcinin kendi içine (register'ına) al."

### Kısıtlama:
Register'da duran bir değişkenin **adresi (&)** olmaz. `&i` yazarsanız derleme hatası alırsınız.

---

## 3. Öğrenme Köşesi: "Modern Derleyiciler Akıllıdır"

Günümüzde GCC ve Clang gibi derleyiciler, hangi fonksiyonun inline yapılacağını ve hangi değişkenin register'a alınacağını sizden çok daha iyi bilirler. Çoğu durumda bu anahtar kelimeleri hiç kullanmamanız, derleyicinin kendi optimizasyonlarını yapmasına izin vermeniz daha iyidir.

---

## 4. StackOverflow Notları: "Premature Optimization" (Erken Optimizasyon)

Donald Knuth'un meşhur sözü: "Erken optimizasyon tüm kötülüklerin anasıdır." Kodunuzun hızını ölçmeden (Profiling) `inline` veya `register` ekleyerek kodu karmaşıklaştırmayın. Önce doğru çalıştırın, sonra yavaşsa hızlandırın.

---

## 5. Gerçek Hayat: Gömülü Sistemler

Modern bilgisayarlarda bu kelimeler etkisiz kalsa da, çok kısıtlı donanımlarda (örneğin 8-bit bir mikrodenetleyici) `register` kullanımı hala hayati bir fark yaratabilir.

---

*Hata durumunda "Büyük Zıplama":* [04-hata-yonetimi-setjmp-longjmp.md](04-hata-yonetimi-setjmp-longjmp.md)
