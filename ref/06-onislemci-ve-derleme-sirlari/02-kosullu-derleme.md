---
baslik: "Koşullu Derleme"
kategori: "derleme"
etiketler: ["ifdef", "ifndef", "debug", "cross-platform", "compiler-flags"]
zorluk: "orta"
ilgili: ["01-makrolar-ve-tehlikeleri.md", "03-header-dosyalari-ve-kapsam.md"]
---

# Koşullu Derleme

Koşullu derleme, aynı kod dosyasından farklı durumlar için farklı programlar üretmenizi sağlar.

## 1. #ifdef ve #ifndef

"Eğer tanımlıysa" veya "tanımlı değilse" anlamına gelirler.

```c
#define DEBUG

int main() {
    #ifdef DEBUG
        printf("Hata ayıklama modu açık.\n");
    #endif
    printf("Program çalışıyor.\n");
}
```
Eğer `#define DEBUG` satırını silerseniz, o `printf` satırı derleyici tarafından **hiç görülmez**. Sanki o satır kodda hiç yokmuş gibi davranılır.

---

## 2. Çoklu Platform Desteği (Cross-Platform)

C, "bir kez yaz, her yerde derle" dilidir. Ama bazen Windows ve Linux farklı fonksiyonlar kullanmanızı gerektirir.

```c
#ifdef _WIN32
    #include <windows.h>
    #define TEMIZLE "cls"
#else
    #include <unistd.h>
    #define TEMIZLE "clear"
#endif

void ekrani_sil() {
    system(TEMIZLE);
}
```

---

## 3. Özellik Kontrolü (#if)

Sayısal karşılaştırmalar yapmanıza olanak tanır.

```c
#define VERSION 2

#if VERSION >= 2
    void yeni_ozellik();
#endif
```

---

## 4. Öğrenme Köşesi: `#error` ile Kısıtlama

Eğer kodunuzun belirli bir standartta derlenmesini istiyorsanız, ön işlemciyi durdurabilirsiniz:
```c
#if __STDC_VERSION__ < 201112L
    #error "Bu program en az C11 standardı gerektirir!"
#endif
```

---

## 5. StackOverflow Notları: "Feature Macros"

Bazen derleme komutuna (`gcc -DDEBUG`) bayrak ekleyerek kodun içindeki `#define` satırlarını kontrol edebilirsiniz. Bu sayede kodun içine dokunmadan "Debug" veya "Release" sürümleri arasında geçiş yapabilirsiniz.

---

## 6. Gerçek Hayat: Kütüphane Uyumluluğu

Büyük kütüphaneler (örneğin OpenGL veya SDL), kullanıcının ekran kartına veya işletim sistemine göre en iyi kodu seçmek için binlerce satır koşullu derleme içerir. Bu sayede aynı oyun hem telefonda, hem bilgisayarda, hem de konsolda çalışabilir.

---

*Kodun yolculuğu:* [03-derleme-asamalari-derinlemesine.md](03-derleme-asamalari-derinlemesine.md)
