---
baslik: "String Manipülasyonu ve Bellek"
kategori: "bellek"
etiketler: ["string", "char", "null-terminator", "strcpy", "strlen"]
zorluk: "orta"
ilgili: ["02-diziler-ve-bellek-yerlesimi.md", "04-dinamik-bellek-yonetimi.md"]
---

# String Manipülasyonu ve Bellek

C'de "String" diye bir veri tipi yoktur. Stringler, sonu özel bir karakterle (`\0` - Null Terminator) biten `char` dizileridir.

## 1. Null Terminator (`\0`) Neden Var?

C'de diziler kendi boyutlarını bilmezler. Bir `char` dizisinin nerede bittiğini anlamak için sonuna "sıfır" değerini koyarız.

**Analoji:** Bir trenin son vagonuna kırmızı bayrak asmak gibi. Makinist (fonksiyon), kırmızı bayrağı görene kadar gitmeye devam eder.

```c
char isim[] = "Cem";
// Bellekte: 'C', 'e', 'm', '\0'  (4 byte yer kaplar!)
```

---

## 2. String Fonksiyonları (`string.h`)

Bu kütüphane, stringlerle çalışmak için araçlar sunar. Ancak çoğu "tehlikelidir" çünkü boyut kontrolü yapmazlar.

| Fonksiyon | Görevi | Risk |
|-----------|--------|------|
| `strlen()` | Boyutu verir (`\0` hariç). | Yok. |
| `strcpy()` | Bir stringi diğerine kopyalar. | **Yüksek!** Hedef dizi küçükse taşar. |
| `strcat()` | İki stringi birleştirir. | **Yüksek!** Hedef dizi küçükse taşar. |
| `strcmp()` | İki stringi karşılaştırır. | Yok. |

---

## 3. Güvenli Alternatifler: `strncpy` ve `strncat`

Profesyonel kodda her zaman "n" harfi içeren versiyonlar kullanılır. Bu "n", "en fazla bu kadar karakter yaz" demektir.

```c
char hedef[5];
strncpy(hedef, "BuCokUzunBirYazi", sizeof(hedef) - 1);
hedef[sizeof(hedef) - 1] = '\0'; // Manuel null koymak güvenlidir.
```

---

## 4. String Literals vs Char Arrays

```c
char *s1 = "Merhaba"; // Salt-okunur (Read-only) bölgededir. Değiştirilemez!
char s2[] = "Merhaba"; // Stack üzerindedir. Değiştirilebilir.

s1[0] = 'H'; // SEGMENTATION FAULT! (Yasaklı bölgeye yazma hatası)
s2[0] = 'H'; // Çalışır.
```

---

## 5. Öğrenme Köşesi: `gets()` Neden Yasaklandı?

`gets()` fonksiyonu kullanıcıdan satır okur ama sınır tanımaz. 1988'deki ilk büyük internet virüsü (**Morris Worm**), `gets()` kullanarak sistemleri ele geçirmişti. C standartlarından tamamen kaldırıldı. Yerine her zaman `fgets()` kullanın.

---

## 6. StackOverflow Notları: `strlen` Maliyeti

`strlen()` fonksiyonu, dizinin başından başlar ve `\0` görene kadar sayar.
```c
for (int i = 0; i < strlen(s); i++) { ... } // ÇOK YAVAŞ!
```
Eğer string 1 milyon karakterse, her turda 1 milyon adım atar. Bunun yerine boyutu bir değişkene kaydedin: `int n = strlen(s);`.

---

## 7. Gerçek Hayat: Buffer Overflow (Tampon Taşması)

Kullanıcıdan "Adınız nedir?" diye sorup 10 karakterlik yer ayırdığınızda, kullanıcı 100 karakter girerse ne olur? Fazladan giren 90 karakter, bellekteki diğer değişkenlerin üstüne yazılır. Bu, siber saldırganların en sevdiği giriş kapısıdır.

---

*Belleği çalışma zamanında yönetmek:* [04-dinamik-bellek-yonetimi.md](04-dinamik-bellek-yonetimi.md)
