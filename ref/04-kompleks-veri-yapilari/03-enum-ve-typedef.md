---
baslik: "Enum ve Typedef"
kategori: "veri-yapilari"
etiketler: ["enum", "typedef", "alias", "sabit", "okunabilirlik"]
zorluk: "baslangic"
ilgili: ["01-struct-ve-bellek-hizalama.md", "02-union-ve-bit-alanlari.md"]
---

# Enum ve Typedef

C'de kod yazmak sadece bilgisayara komut vermek değil, başka insanlara (ve 6 ay sonraki kendinize) ne yaptığınızı anlatmaktır. `enum` ve `typedef` bu iletişimi kolaylaştırır.

## 1. enum (Numaralandırma)

Anlamı olmayan sayılar yerine, anlamlı isimler kullanmanızı sağlar.

```c
enum Renk {
    KIRMIZI, // Arka planda 0
    YESIL,   // Arka planda 1
    MAVI     // Arka planda 2
};

enum Renk favorim = YESIL;
```

### İşleyiş:
`enum` aslında bir tam sayıdır (int). Derleyici arka planda `YESIL` gördüğü her yere `1` yazar. Ama siz kodda `1` yerine `YESIL` görerek ne yapıldığını çok daha iyi anlarsınız.

---

## 2. typedef (Tip Tanımlama)

Var olan bir tipe yeni (ve daha kısa) bir isim (alias) vermenizi sağlar. Tipi değiştirmez, sadece adını değiştirir.

**Analoji:** "Türkiye Cumhuriyeti Kimlik Numarası" yerine kısaca "TC No" demek gibi.

```c
typedef unsigned long long u64;
u64 sayi = 100; // Artık her seferinde o uzun ismi yazmanıza gerek yok.
```

---

## 3. En Yaygın Kullanım: Typedef Struct

Struct kullanırken her seferinde `struct` anahtar kelimesini yazmak yorucudur. `typedef` ile bu sorunu çözeriz.

```c
typedef struct {
    int x;
    int y;
} Nokta;

Nokta p1; // 'struct Nokta p1' yazmaktan çok daha temiz.
```

---

## 4. Öğrenme Köşesi: Enum Değerlerini Manuel Atamak

Enum değerleri her zaman 0'dan başlamak zorunda değildir.
```c
enum Hata {
    OK = 0,
    NOT_FOUND = 404,
    SERVER_ERROR = 500
};
```

---

## 5. StackOverflow Notları: "Typedef Pointers"

```c
typedef int* IntPtr;
IntPtr p1, p2; // İkisi de pointer'dır.
```
Normalde `int *p1, p2;` yazarsanız `p2` sadece bir `int` olur. `typedef` bu tür kafa karışıklıklarını önlemek için harika bir yoldur.

---

## 6. Gerçek Hayat: Boole (Mantıksal) Tip

Eski C standartlarında (C89) `bool` tipi yoktu. Yazılımcılar kendi `bool` tiplerini şöyle yaparlardı:
```c
typedef enum { FALSE, TRUE } bool;
```
Modern C'de ise `<stdbool.h>` kütüphanesi tam olarak buna benzer bir işi sizin yerinize yapar.

---

*Veriyi zincirleme bağlamak:* [04-bagli-listeler-ve-otesis.md](04-bagli-listeler-ve-otesis.md)
