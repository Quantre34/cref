---
baslik: "Header Dosyaları ve Kapsam"
kategori: "moduler"
etiketler: ["header", "include", "h-dosyasi", "include-guard", "moduler"]
zorluk: "orta"
ilgili: ["01-fonksiyonlar-ve-stack-frame.md", "06-onislemci-ve-derleme-sirlari/01-makrolar-ve-tehlikeleri.md"]
---

# Header Dosyaları ve Kapsam

Büyük projelerde binlerce satır kodu tek bir dosyaya yazmak imkansızdır. Kodu mantıklı parçalara ayırıp birbirine bağlamamız gerekir.

## 1. .c vs .h (Giriş ve Katalog)

- **.c Dosyaları:** İşin yapıldığı yerdir (Uygulama).
- **.h Dosyaları:** Nelerin yapılabileceğinin listesidir (Katalog/Arayüz).

**Analoji:** Bir restorana gittiğinizde size verilen **menü** header (`.h`) dosyasıdır. Mutfağın içindeki **yemek yapma süreci** ise kaynak (`.c`) dosyasıdır.

---

## 2. Include Guard (İçerme Koruması)

Bir dosyayı yanlışlıkla iki kez dahil ederseniz (örneğin `A.h`, `B.h`'ı içeriyor ve siz her ikisini de `main.c`'de içeriyorsanız), fonksiyon tanımları çakışır ve hata alırsınız. Bunu önlemek için **Include Guard** kullanılır.

```c
// math_util.h
#ifndef MATH_UTIL_H    // Eğer bu isim tanımlı değilse...
#define MATH_UTIL_H    // Tanımla.

int topla(int a, int b);

#endif                 // Bitti.
```

Modern bir alternatif: `#pragma once` (Çoğu derleyici destekler).

---

## 3. < > vs " " (Kütüphane mi, Benim mi?)

- `#include <stdio.h>`: Sistem kütüphaneleri için. Derleyici bunları standart klasörlerde arar.
- `#include "benim_kodum.h"`: Kendi dosyaların için. Derleyici önce mevcut klasöre bakar.

---

## 4. Öğrenme Köşesi: "Multiple Definition" Hatası

Header dosyalarına asla fonksiyonun **gövdesini** yazmayın.
```c
// HATALI (header dosyası)
int topla(int a, int b) { return a + b; }
```
Eğer bu header'ı iki farklı `.c` dosyasında içerirseniz, bağlayıcı (linker) "İki tane `topla` fonksiyonu var, hangisini kullanayım?" der ve hata verir. Header'a sadece **prototip** yazılır.

---

## 5. StackOverflow Notları: "Circular Dependency" (Döngüsel Bağımlılık)

`A.h` dosyası `B.h`'ı istiyor, `B.h` da `A.h`'ı istiyor. Bu bir kısır döngüdür.
- **Çözüm:** "Forward Declaration" kullanın. Yani dosyanın tamamını dahil etmek yerine, sadece "Böyle bir tip var" diyerek geçiştirin.

---

## 6. Gerçek Hayat: API Tasarımı

Şirketler (örneğin Google veya NVIDIA) size kütüphanelerini verirken sadece `.h` dosyalarını ve derlenmiş `.lib`/`.so` dosyalarını verirler. Kaynak kodlarını (`.c`) gizli tutarlar. Siz sadece `.h` dosyasına bakarak o kütüphaneyi nasıl kullanacağınızı anlarsınız.

---

*Değişkenlerin görünürlüğünü kontrol etmek:* [04-static-ve-extern.md](04-static-ve-extern.md)
