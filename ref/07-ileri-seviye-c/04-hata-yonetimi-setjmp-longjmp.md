---
baslik: "Hata Yönetimi (setjmp ve longjmp)"
kategori: "ileri"
etiketler: ["setjmp", "longjmp", "error-handling", "exception", "context-switch"]
zorluk: "ileri"
ilgili: ["01-fonksiyonlar-ve-stack-frame.md", "17-yaygin-hatalar.md"]
---

# Hata Yönetimi (setjmp ve longjmp)

C'de `try-catch` (hata yakalama) mekanizması yoktur. Ancak buna çok benzer bir işi yapan, fonksiyon sınırlarını aşarak "zıplamanızı" sağlayan `setjmp` ve `longjmp` fonksiyonları vardır.

## 1. Büyük Zıplama (Non-local Jump)

Normalde `return` ile sadece bir üst fonksiyona dönebilirsiniz. `longjmp` ile ise 10 kat derinlikteki bir fonksiyondan en baştaki "güvenli noktaya" tek seferde zıplayabilirsiniz.

```c
#include <setjmp.h>

jmp_buf guvenli_nokta;

void alt_fonksiyon() {
    if (hata_olustu) {
        longjmp(guvenli_nokta, 1); // Buradan doğrudan setjmp'ye zıpla!
    }
}

int main() {
    if (setjmp(guvenli_nokta) == 0) { // 1. Noktayı işaretle
        alt_fonksiyon();
    } else { // 2. Zıplama gerçekleştiyse buraya düşer
        printf("Bir hata yakalandı!");
    }
}
```

---

## 2. İşleyiş: İşlemci Kayıtlarını Yedeklemek

- **setjmp:** O anki işlemci register'larını (stack pointer, instruction pointer vb.) `jmp_buf` içine kaydeder.
- **longjmp:** Kaydedilen bu register'ları geri yükler. İşlemci kendini bir anda `setjmp`'nin olduğu satırda bulur.

---

## 3. Büyük Tehlike: Bellek Sızıntısı ve Local Değişkenler

`longjmp` ile zıpladığınızda, arada kalan fonksiyonlardaki yerel değişkenlerin Stack Frame'leri bir anda yok olur.
- **Risk 1:** Eğer o fonksiyonlarda `malloc` yapmışsanız ve `free` etmeden zıplarsanız, o bellek sonsuza dek sızar (Leak).
- **Risk 2:** `setjmp` olan fonksiyondaki yerel değişkenlerin değeri, zıplama sonrası belirsiz olabilir (Derleyici optimizasyonları yüzünden). Bu değişkenleri `volatile` yapmak güvenlidir.

---

## 4. Öğrenme Köşesi: "goto"dan Farkı Nedir?

`goto` sadece **aynı fonksiyon içinde** bir yere zıplayabilir. `setjmp/longjmp` ise **fonksiyonlar arası** (farklı stack frameler arası) zıplayabilir.

---

## 5. StackOverflow Notları: "C++ Exceptions"

C++'daki `try-catch` mekanizması aslında arka planda çok daha gelişmiş ve güvenli bir `setjmp/longjmp` benzeri yapı kullanır. C'de bunu manuel yapmak çok dikkat gerektirir.

---

## 6. Gerçek Hayat: Gömülü Yazılımlar ve Sinyaller

Kritik bir hata oluştuğunda (örneğin sıfıra bölme veya geçersiz bellek erişimi), işletim sistemi sinyal gönderir. Bu sinyali yakalayıp programı tamamen kapatmak yerine, `longjmp` ile ana menüye veya güvenli bir duruma dönmek için kullanılır.

---

*Hangi kütüphane ne işe yarar?* [08-standart-kutuphane-referansi/stdio-detay.md](../08-standart-kutuphane-referansi/stdio-detay.md)
