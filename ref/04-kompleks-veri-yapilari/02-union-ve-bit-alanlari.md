---
baslik: "Union ve Bit Alanları"
kategori: "veri-yapilari"
etiketler: ["union", "bit-field", "bellek", "tasarruf", "register"]
zorluk: "ileri"
ilgili: ["01-struct-ve-bellek-hizalama.md", "15-bit-islemleri.md"]
---

# Union ve Bit Alanları

`union` ve **Bit Alanları**, C'nin belleğin en ince ayrıntılarına kadar hükmetmenizi sağlayan araçlarıdır. Genellikle bellek tasarrufu ve donanım kontrolü için kullanılırlar.

## 1. Union (Birlik): Aynı Odada Sırayla Oturmak

`union`, içindeki tüm değişkenlerin **aynı bellek adresinden** başlamasını sağlar. Boyutu, içindeki en büyük değişkenin boyutu kadardır.

**Analoji:** Bir telefon kulübesi düşünün. İçine bir basketbolcu (int) veya bir çocuk (char) girebilir. Ama ikisi aynı anda giremez. Kim en son girdiyse, kulübe ona ait olur.

```c
union Veri {
    int i;
    float f;
    char str[20];
};
```
Eğer `i`'ye değer atarsanız, sonra `f`'ye değer atarsanız, `i`'nin değeri bozulur.

---

## 2. Neden Union Kullanırız?

1. **Bellek Tasarrufu:** Bir nesne aynı anda ya A ya da B tipindeyse, ikisi için ayrı yer ayırmak yerine aynı yeri kullanmak mantıklıdır.
2. **Tip Dönüşümü (Type Punning):** Bir `float` sayısının içindeki bitleri bir `int` gibi okumak isterseniz `union` harikadır.

---

## 3. Bit Alanları (Bit-Fields)

Bazen bir `int` (32 bit) içindeki sadece 1 veya 2 bitlik bilgiye ihtiyacımız olur (örneğin Evet/Hayır cevabı). Normalde 32 bit harcamak yerine, her bitin neyi temsil edeceğini belirleyebiliriz.

```c
struct Durum {
    unsigned int lamba_acik : 1; // Sadece 1 bit kullan.
    unsigned int fan_hizi   : 3; // Sadece 3 bit (0-7 arası) kullan.
    unsigned int hata_kodu  : 4; // Sadece 4 bit kullan.
};
```
Bu koca struct bellekte sadece 1 byte (8 bit) kaplayabilir!

---

## 4. Öğrenme Köşesi: Union ile Küçük-Büyük Endian Testi

İşlemcinizin sayıları belleğe ters mi düz mü yazdığını anlamak için `union` kullanabilirsiniz:
```c
union {
    int i;
    char c[4];
} test;
test.i = 1;
if (test.c[0] == 1) printf("Little Endian"); // Intel/AMD genelde böyledir.
```

---

## 5. StackOverflow Notları: "Undefined Behavior" Riski

`union` içindeki bir değişkene yazıp, başka bir tipteki değişkenden okumak (örneğin `float` yazıp `int` okumak) bazı durumlarda C standardına göre "Tanımsız Davranış"tır. Ancak sistem programlamada (Kernel, Driver) sıkça başvurulan bir yöntemdir.

---

## 6. Gerçek Hayat: Donanım Kontrolü

Mikrodenetleyicilerde (Arduino, STM32 vb.) bir donanım register'ı genellikle 32 bittir ve her bit farklı bir özelliği temsil eder. `union` ve `struct` bit alanlarını birleştirerek bu donanımı yönetmek çok kolaylaşır:

```c
union LED_KONTROL {
    uint32_t register_degeri;
    struct {
        uint32_t kirmizi : 1;
        uint32_t yesil   : 1;
        uint32_t mavi    : 1;
    } renkler;
};
```

---

*Kodun okunabilirliğini artırmak:* [03-enum-ve-typedef.md](03-enum-ve-typedef.md)
