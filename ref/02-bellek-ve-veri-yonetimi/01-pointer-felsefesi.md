---
baslik: "Pointer (İşaretçi) Felsefesi"
kategori: "bellek"
etiketler: ["pointer", "adres", "bellek", "referans", "dereference"]
zorluk: "orta"
ilgili: ["02-degiskenler-ve-bellek.md", "02-diziler-ve-bellek-yerlesimi.md"]
---

# Pointer (İşaretçi) Felsefesi

C'yi diğer dillerden ayıran en güçlü (ve en tehlikeli) özellik Pointer'lardır. Pointer, içinde bir **değer** değil, başka bir verinin **bellek adresini** tutan bir değişkendir.

## 1. Adres Kavramı (Mental Model)

Belleği (RAM) devasa bir şehir, her byte'ı ise bir ev olarak düşünün.
- **Değişken:** Evin içindeki eşyadır.
- **Adres (&):** Evin kapı numarasıdır.
- **Pointer (*):** Üzerinde kapı numarası yazılı olan bir kâğıt parçasıdır.

```c
int sayi = 42;
int *ptr = &sayi; // ptr, sayi'nin kapı numarasını (adresini) tutuyor.
```

---

## 2. Temel Operatörler: & ve *

Bu iki sembol pointer dünyasının anahtarlarıdır:

| Operatör | Adı | Görevi | Analoji |
|----------|-----|--------|---------|
| `&` | Address-of | Değişkenin adresini verir. | "Bu evin adresi nedir?" |
| `*` | Dereference | Adresteki değere ulaşır. | "Bu adresteki eve gir ve içindekini getir." |

```c
printf("%p", &sayi); // Adresi yazdırır (Örn: 0x7fff...)
printf("%d", *ptr);  // ptr'nin tuttuğu adrese gider ve 42'yi bulur.
```

---

## 3. Neden Pointer Kullanırız?

1. **Performans:** Büyük bir veriyi (örneğin 1 GB'lık bir fotoğrafı) bir fonksiyona kopyalamak yerine, sadece adresini (8 byte) göndermek çok daha hızlıdır.
2. **Doğrudan Müdahale:** Fonksiyonların içinden, dışarıdaki bir değişkeni kalıcı olarak değiştirebiliriz.
3. **Dinamik Bellek:** Program çalışırken ihtiyaç duyduğumuz kadar yer ayırmamızı sağlar.

---

## 4. Pointer Aritmetiği

Pointer'lar üzerinde toplama/çıkarma yapabilirsiniz ama bu normal matematik değildir.

```c
int *ptr = 1000; // Varsayalım adres 1000
ptr++;           // Sonuç 1004 olur (Çünkü int 4 byte'tır!)
```
- **İşleyiş:** Bir pointer'ı artırdığınızda, bir sonraki "ev"e geçer. Evin boyutu, pointer'ın tipine (int, char, double) bağlıdır.

---

## 5. Öğrenme Köşesi: NULL Pointer

Hiçbir yeri göstermeyen pointer'a `NULL` denir.
```c
int *ptr = NULL;
```
**Analoji:** Üzerinde "Burası boş" yazan bir kâğıt. Bu adrese gitmeye (dereference) çalışırsanız programınız **Segmentation Fault** hatasıyla çöker. Her zaman pointer'ı kullanmadan önce `if (ptr != NULL)` kontrolü yapmak profesyonel bir alışkanlıktır.

---

## 6. StackOverflow Notları: "Wild Pointers"

```c
int *ptr; // Başlatılmamış pointer
*ptr = 10; // FACİA!
```
Bu pointer rastgele bir yeri gösteriyor olabilir. İşletim sisteminin kritik bir bölgesine yazmaya çalışabilir ve sistemi çökertebilirsiniz. Pointer'ları ya `NULL` ile ya da geçerli bir adresle başlatın.

---

## 7. Gerçek Hayat: Donanım Register'ları

Gömülü sistemlerde bir LED'i yakmak için genellikle belirli bir bellek adresine `1` yazmanız gerekir.
```c
volatile uint32_t *led_adresi = (uint32_t *)0x40021000;
*led_adresi = 1; // LED yandı!
```
Pointer'lar donanımla konuşmanın tek yoludur.

---

*Pointer'ların en yakın dostu:* [02-diziler-ve-bellek-yerlesimi.md](02-diziler-ve-bellek-yerlesimi.md)
