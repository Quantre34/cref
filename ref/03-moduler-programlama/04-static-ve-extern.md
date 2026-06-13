---
baslik: "Static ve Extern (Linkage)"
kategori: "moduler"
etiketler: ["static", "extern", "linkage", "global", "kapsam"]
zorluk: "ileri"
ilgili: ["03-header-dosyalari-ve-kapsam.md", "02-degiskenler-ve-bellek.md"]
---

# Static ve Extern (Linkage)

C'de değişkenlerin ve fonksiyonların sadece nerede yaşadığı değil, kimler tarafından görülebileceği de önemlidir. Buna **Linkage** (Bağlantı) denir.

## 1. static: "Burası Benim Özelim"

`static` anahtar kelimesi kullanıldığı yere göre iki farklı anlama gelir:

### 1.1. Dosya Bazlı (Global static)
Bir fonksiyonu veya global değişkeni `static` yaparsanız, o dosyanın dışından kimse ona erişemez.
```c
static int gizli_sayi = 42; // Sadece bu .c dosyasında görünür.
```
**Neden?** İsim çakışmalarını önlemek ve kapsülleme (Encapsulation) sağlamak için.

### 1.2. Fonksiyon İçinde (Local static)
Değişken fonksiyon bitse bile değerini korur (Bellekte ölmez).
```c
void sayac() {
    static int s = 0;
    s++;
    printf("%d", s);
}
// Her çağrıldığında artmaya devam eder: 1, 2, 3...
```

---

## 2. extern: "Başka Yerde Var, İnan Bana"

`extern`, derleyiciye şunu söyler: "Bu değişken burada tanımlı değil ama başka bir dosyada var. Bağlama (linking) aşamasında onu bulacaksın."

```c
// main.c
extern int skor; // skor değişkeni başka bir dosyada (ornegin game.c) tanimli.
```

---

## 3. Öğrenme Köşesi: "Static vs Global"

- **Global:** "Herkes bana ulaşabilir, ben bir ünlüyüm."
- **Static Global:** "Sadece ailem (dosyam) beni tanır, dış dünyaya kapalıyım."

**Deney:**
İki farklı `.c` dosyasında aynı isimle `int x = 10;` tanımlayın ve derleyin. "Multiple definition" hatası alırsınız. Şimdi ikisini de `static int x = 10;` yapın. Sorunsuz derlenecektir.

---

## 4. StackOverflow Notları: "Externed Constants"

Genellikle sabitleri (constants) header dosyasında `extern const int MAX_HIZ;` şeklinde bildirip, bir `.c` dosyasında `const int MAX_HIZ = 300;` şeklinde tanımlamak en temiz yoldur.

---

## 5. Gerçek Hayat: Gizli Fonksiyonlar

Büyük bir kütüphane yazarken (örneğin bir şifreleme kütüphanesi), kütüphanenin içinde kullandığınız yardımcı fonksiyonları (örneğin `karakter_kaydir()`) `static` yapmalısınız. Kullanıcı sadece ana fonksiyonu (`sifrele()`) görmelidir. Bu, kütüphanenizin kullanımını kolaylaştırır ve hata payını düşürür.

---

*Veriyi daha karmaşık yapılarla organize etmek:* [04-kompleks-veri-yapilari/01-struct-ve-bellek-hizalama.md](../04-kompleks-veri-yapilari/01-struct-ve-bellek-hizalama.md)
