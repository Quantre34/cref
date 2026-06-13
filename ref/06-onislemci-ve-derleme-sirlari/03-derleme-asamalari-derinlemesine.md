---
baslik: "Derleme Aşamaları Derinlemesine"
kategori: "derleme"
etiketler: ["gcc", "preprocessing", "compilation", "assembly", "linking"]
zorluk: "ileri"
ilgili: ["00-kurulum-ve-hazirlik.md", "01-makrolar-ve-tehlikeleri.md"]
---

# Derleme Aşamaları Derinlemesine

Yazdığınız `.c` dosyasının çalıştırılabilir bir program haline gelmesi için 4 temel aşamadan geçmesi gerekir. Bu süreci anlamak, karmaşık hataları (özellikle bağlayıcı hatalarını) çözmenizi sağlar.

## 1. Ön İşleme (Preprocessing)

Derleyiciden önce "Ön İşlemci" sahneye çıkar.
- `#include` ile belirtilen dosyaları koda yapıştırır.
- `#define` makrolarını yerlerine yazar.
- Yorumları temizler.
- **Çıktı:** `.i` dosyası (Hala C kodudur ama çok uzundur).

---

## 2. Derleme (Compilation)

Ön işlemden geçmiş temiz C kodunu, işlemcinin anlayabileceği düşük seviyeli **Assembly** diline dönüştürür.
- Sözdizimi (Syntax) hataları bu aşamada yakalanır.
- Optimizasyonlar (`-O2`, `-O3`) burada yapılır.
- **Çıktı:** `.s` dosyası (Assembly kodu).

---

## 3. Birleştirme (Assembly)

Assembly kodunu, makine koduna (0 ve 1) dönüştürür.
- **Çıktı:** `.o` (veya `.obj`) dosyası. Buna **Object File** (Nesne Dosyası) denir.
- **Not:** Bu dosya henüz çalışamaz çünkü diğer fonksiyonların (örneğin `printf`) tam olarak nerede olduğunu bilmez.

---

## 4. Bağlama (Linking)

Farklı `.o` dosyalarını ve kütüphaneleri birleştirerek tek bir çalıştırılabilir dosya üretir.
- Fonksiyonların adreslerini birbirine bağlar.
- **Çıktı:** Çalıştırılabilir dosya (`a.out` veya `.exe`).

---

## 5. Öğrenme Köşesi: "Undefined Reference" vs "Implicit Declaration"

- **Implicit Declaration:** Derleme (2. aşama) hatasıdır. "Bu fonksiyonun tipini söylemedin" der.
- **Undefined Reference:** Bağlama (4. aşama) hatasıdır. "Fonksiyonun tipini biliyorum ama kendisini bulamıyorum" der.

---

## 6. StackOverflow Notları: "Compiler" vs "Linker"

Yazılımcılar genellikle tüm sürece "derleme" derler ama hatayı kimin verdiğini bilmek önemlidir. Eğer hata mesajında satır numarası varsa derleyiciden (Compiler), yoksa sadece fonksiyon ismi varsa bağlayıcıdan (Linker) geliyordur.

---

## 7. Gerçek Hayat: Statik ve Dinamik Bağlama

- **Statik Bağlama:** Kütüphane kodunu doğrudan programın içine gömer. Programın boyutu büyür ama her yerde çalışır.
- **Dinamik Bağlama (.dll / .so):** Kütüphaneyi programın dışında tutar. Program küçüktür ama çalışması için o `.dll` dosyasının sistemde kurulu olması gerekir.

---

*C'nin sınırlarını zorlamak:* [07-ileri-seviye-c/01-fonksiyon-pointerlari.md](../07-ileri-seviye-c/01-fonksiyon-pointerlari.md)
