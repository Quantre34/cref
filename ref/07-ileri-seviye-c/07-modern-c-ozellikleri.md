---
baslik: "C11/C23 Modern C Özellikleri"
kategori: "ileri"
etiketler: ["_Generic", "static_assert", "nullptr", "c11", "c23"]
zorluk: "ileri"
ilgili: ["01-temel-programlama/07-c-standartlari-ve-tarihce.md"]
---

# C11/C23 Modern C Özellikleri

C sadece 70'lerin dili değildir. Modern standartlarla (C11, C23) birlikte programcılığı kolaylaştıran çok güçlü özellikler gelmiştir.

## 1. _Generic (C11 - Compile-time Overloading)

Farklı tipler için aynı isimli makro yazmanızı sağlar.

```c
#define yazdir(x) _Generic((x), \
    int: printf("%d\n", (x)), \
    double: printf("%f\n", (x)), \
    char*: printf("%s\n", (x)) \
)

yazdir(5);      // int çalışır
yazdir("Selam"); // char* çalışır
```

---

## 2. Static Assertion (C11 - Derleme Anı Kontrolü)

Kodun çalışmasına gerek kalmadan, derleme sırasında bir kuralı kontrol eder. Eğer kural tutmazsa program derlenmez.

```c
_Static_assert(sizeof(int) == 4, "Bu sistemde int 4 byte olmalı!");
```

---

## 3. nullptr (C23)

Yıllardır beklenen `nullptr`, artık C'ye de geldi. Eskiden `0` veya `(void*)0` (NULL) kullanılırdı. `nullptr` tip güvenliği sağlar ve pointer hatalarını azaltır.

---

## 4. typeof (C23)

Bir değişkenin tipini otomatik olarak almanızı sağlar. Makro yazarken hayat kurtarır.
```c
int a = 10;
typeof(a) b = 20; // b de int olur.
```

---

## 5. Öğrenme Köşesi: "Alignas" ve Bellek Yerleşimi

C11 ile gelen `_Alignas` ve `_Alignof` ile, bir değişkenin bellekte tam olarak hangi sınırda duracağını belirleyebilirsiniz. Bu, özel donanım sürücüleri (driver) yazarken çok önemlidir.

---

## 6. StackOverflow Notları: "C++ ile Farklar"

C23 ile C, C++'a bir adım daha yaklaştı; ancak C hala "Zero-overhead" felsefesini korur. C++'daki gibi karmaşık sınıf (class) yapıları yerine, bu yeni özellikleri verimliliği artırmak için kullanır.

---

*Hata yönetimi:* [07-ileri-seviye-c/04-hata-yonetimi-setjmp-longjmp.md](04-hata-yonetimi-setjmp-longjmp.md)
