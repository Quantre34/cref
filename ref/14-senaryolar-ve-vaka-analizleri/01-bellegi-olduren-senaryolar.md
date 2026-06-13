---
baslik: "Belleği Öldüren 10 Senaryo"
kategori: "senaryo"
etiketler: ["memory-leak", "heap-overflow", "fragmentation", "debugging"]
zorluk: "ileri"
ilgili: ["02-bellek-ve-veri-yonetimi/04-dinamik-bellek-yonetimi.md"]
---

# Belleği Öldüren 10 Senaryo

Sadece `malloc` ve `free` bilmek yetmez. Gerçek hayatta belleği nasıl "öldürdüğünü" anlaman için karşına çıkacak en acı 10 senaryoyu inceleyelim.

## 1. Döngü İçinde Unutulan `free()`
**Hata:** Her saniye çalışan bir döngüde 1 KB yer ayırıp serbest bırakmamak.
```c
while(1) {
    char *buf = malloc(1024);
    // ... işlem yap ...
    // free(buf); unutuldu!
}
```
**Sonuç:** Programın birkaç saat içinde GB'larca RAM tüketip sistemi dondurması.

## 2. Pointer'ın Üzerine Yazmak (Lost Pointer)
**Hata:** `free` etmeden pointer'a yeni bir adres atamak.
```c
char *p = malloc(100);
p = malloc(200); // İlk 100 byte artık sahipsiz ve ulaşılamaz!
```
**Sonuç:** Kesin bellek sızıntısı (Definite leak).

## 3. `realloc` Başarısız Olduğunda Veriyi Kaybetmek
**Hata:** `p = realloc(p, size)` yazmak.
```c
char *p = malloc(100);
p = realloc(p, 200000000000); // realloc NULL dönerse p'nin eski adresi silinir!
```
**Sonuç:** Hem sızıntı, hem de verinin kaybı. **Çözüm:** Geçici bir pointer kullanın.

## 4. Struct İçindeki Pointerları Unutmak
**Hata:** Struct'ı `free` edip içindeki dinamik alanları etmemek.
```c
typedef struct { char *isim; } Ogrenci;
Ogrenci *o = malloc(sizeof(Ogrenci));
o->isim = malloc(50);
free(o); // o->isim hala bellekte sızıyor!
```

## 5. String Literalleriyle Karıştırmak
**Hata:** Dinamik ayrılmış bir pointer'a string literali atamak.
```c
char *s = malloc(20);
s = "Sabit Metin"; // Bellek sızdı + s artık free() edilemez!
```

## 6. Fonksiyon Dönüşünde Adresi Kaybetmek
**Hata:** Fonksiyon içinde `malloc` yapıp adresi geri döndürmemek veya saklamamak.

## 7. Bellek Parçalanması (Fragmentation)
**Hata:** Milyonlarca çok küçük (1-2 byte) yer ayırıp serbest bırakmak.
**Sonuç:** RAM'de toplamda boş yer olsa bile, yan yana (bitişik) büyük bir yer kalmaz ve program yeni yer ayıramaz.

## 8. Yanlış `free()` Kullanımı
**Hata:** `malloc` ile alınmamış (örn: Stack'teki) bir adresi `free` etmeye çalışmak.
```c
int x; free(&x); // PROGRAM ÇÖKER.
```

## 9. Çoklu `free()` (Double Free)
**Hata:** Aynı adresi iki kez serbest bırakmak. Genelde karmaşık `if-else` yapılarında olur.

## 10. Dizi Boyutunu Yanlış Hesaplamak (Off-by-one)
**Hata:** `sizeof(dizi)` yerine `sizeof(ptr)` kullanarak yer ayırmak (veya tersi).

---

## Öğrenme Köşesi: "Kiralama Sözleşmesi"
`malloc` bir kiralama sözleşmesidir. Evi (belleği) teslim ettiğinde (`free`), elindeki anahtarı (pointer) da kırmalısın (`ptr = NULL`). Yoksa eski eve girmeye çalışıp hırsız muamelesi görürsün (Segfault).

---

*Hataları tespit et:* [02-pointer-kabuslari.md](02-pointer-kabuslari.md)
