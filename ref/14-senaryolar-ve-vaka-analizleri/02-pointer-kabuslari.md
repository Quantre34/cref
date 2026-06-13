---
baslik: "Pointer Kabusları ve Aliasing"
kategori: "senaryo"
etiketler: ["pointer", "aliasing", "dangling-pointer", "void-pointer", "casting"]
zorluk: "ileri"
ilgili: ["02-bellek-ve-veri-yonetimi/01-pointer-felsefesi.md"]
---

# Pointer Kabusları ve Aliasing

Pointer'lar C'nin en güçlü silahıdır ama namlusu bazen sana dönebilir. İşte pointer kullanırken uykunu kaçıracak 5 vaka analizi.

## 1. Dangling Pointer (Sarkan İşaretçi)
**Vaka:** Bir fonksiyon içindeki yerel bir değişkenin adresini döndürmek.
```c
int* tehlikeli_fonksiyon() {
    int x = 10;
    return &x; // HATA! x fonksiyon bitince ölür.
}
```
**Neden Kabus?** Sen bu adrese ulaştığında orada hala `10` görebilirsin (şansına), ama bir sonraki fonksiyon çağrısında o veri çoktan değişmiş olacaktır.

## 2. Pointer Aliasing (Takma İsim Problemi)
**Vaka:** İki farklı pointer'ın aynı bellek bölgesini göstermesi.
```c
void kopyala(int *a, int *b, int *n) {
    *n = *a + *b;
}
// Eğer n ve a aynı adresi gösteriyorsa sonuç beklediğinden farklı çıkar!
```

## 3. Yanlış Tür Dönüşümü (Incorrect Casting)
**Vaka:** Bir `int*` pointer'ını `double*` gibi kullanmak.
```c
int x = 10;
double *d = (double*)&x;
*d = 3.14; // HATA! int 4 byte, double 8 byte. Yan taraftaki 4 byte'ı da bozdun.
```

## 4. Unaligned Access (Hizalanmamış Erişim)
**Vaka:** Bazı işlemcilerde (Örn: ARM) bir `int`'in adresi mutlaka 4'ün katı olmalıdır. Eğer bir pointer'ı manuel olarak tek sayılı bir adrese çekip okuma yaparsan donanım seviyesinde hata (Bus Error) alırsın.

## 5. Void Pointer Gizemi
**Vaka:** `void *` kullanırken cast etmeyi unutmak veya yanlış tipte cast etmek. `void*` üzerinde aritmetik (`p++`) yapamazsın çünkü "boşluğun boyutu" yoktur.

---

## Öğrenme Köşesi: "Pointer Güvenlik Kontrol Listesi"
1. Pointer'ı tanımlarken `NULL` yap.
2. `free()` ettikten sonra `NULL` yap.
3. Asla yerel (`stack`) adresini geri döndürme.
4. `malloc`'tan sonra dönen adresi `if(p == NULL)` ile kontrol et.

---

*Hataları ayıklama sanatı:* [03-vaka-analizi-segfault-otopsisi.md](03-vaka-analizi-segfault-otopsisi.md)
