---
baslik: "Struct (Yapı) ve Bellek Hizalama"
kategori: "veri-yapilari"
etiketler: ["struct", "bellek", "padding", "alignment", "hizalama"]
zorult: "orta"
ilgili: ["02-degiskenler-ve-bellek.md", "02-union-ve-bit-alanlari.md"]
---

# Struct (Yapı) ve Bellek Hizalama

`struct`, birbiriyle ilgili farklı tipteki değişkenleri tek bir çatı altında toplamamızı sağlar. Ancak C, bu değişkenleri belleğe dizerken bazen araya "boşluklar" koyar.

## 1. Struct Nedir? (Mental Model)

Bir `struct`'ı, farklı bölmeleri olan bir **çekmece** gibi düşünün. Her bölmenin boyutu farklı olabilir ama hepsi aynı çekmecenin içindedir.

```c
struct Ogrenci {
    char isim[20];
    int yas;
    float ortalama;
};
```

---

## 2. Bellek Hizalama (Memory Alignment)

İşlemciler belleği her zaman byte-byte okumaz. Genellikle 4 veya 8 byte'lık bloklar halinde okumayı tercih ederler (hız için). Bu yüzden derleyici, değişkenleri bu bloklara denk getirmek için araya boş byte'lar ekler. buna **Padding** denir.

### Örnek:
```c
struct Veri {
    char c;    // 1 byte
    // [3 byte boşluk/padding]
    int i;     // 4 byte
};
```
Burada `sizeof(struct Veri)` 5 değil, **8** çıkar! Çünkü `int`, 4'ün katı olan bir adreste durmak ister.

---

## 3. Padding'i Optimize Etmek

Değişkenlerin sırasını değiştirerek bellekten tasarruf edebilirsiniz.

```c
// KÖTÜ (12 byte)
struct A { char c; int i; char d; };

// İYİ (8 byte)
struct B { int i; char c; char d; };
```
**Kural:** Büyükten küçüğe doğru sıralamak genellikle padding'i minimize eder.

---

## 4. Öğrenme Köşesi: "Self-Referencing Struct"

Bir struct kendi içinde kendi tipinden bir pointer tutabilir. Bu, bağlı listelerin (linked lists) temelidir.
```c
struct Dugum {
    int veri;
    struct Dugum *sonraki;
};
```

---

## 5. StackOverflow Notları: `sizeof` vs Toplam

Asla "içindeki tiplerin boyutlarını toplayıp" struct boyutunu hesaplamayın. Her zaman `sizeof(struct Isim)` kullanın. Aksi takdirde padding yüzünden yanlış bellek ayırırsınız (malloc ile).

---

## 6. Gerçek Hayat: Paketleme (Pragma Pack)

Gömülü sistemlerde veya bir ağ paketini doğrudan struct'a eşlemek istediğinizde padding istemezsiniz. Derleyiciye "araya boşluk koyma!" diyebilirsiniz:

```c
#pragma pack(push, 1)
struct Paket {
    char tip;
    int deger;
};
#pragma pack(pop)
// Artık tam 5 byte kaplar.
```
**Dikkat:** Bu işlem programınızı biraz yavaşlatabilir çünkü işlemci artık "hizalanmamış" (unaligned) veriyi okumak için daha çok uğraşır.

---

*Benzer bellek alanını paylaşmak:* [02-union-ve-bit-alanlari.md](02-union-ve-bit-alanlari.md)
