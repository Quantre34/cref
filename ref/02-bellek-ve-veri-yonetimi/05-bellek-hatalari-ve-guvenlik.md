---
baslik: "Bellek Hataları ve Güvenlik"
kategori: "bellek"
etiketler: ["segfault", "memory-leak", "buffer-overflow", "security", "valgrind"]
zorluk: "orta"
ilgili: ["04-dinamik-bellek-yonetimi.md", "17-yaygin-hatalar.md"]
---

# Bellek Hataları ve Güvenlik

C'de bellek yönetimi elledir. Bu size büyük bir hız kazandırır ama en küçük bir dikkatsizlik tüm sistemin çökmesine veya hacklenmesine yol açabilir.

## 1. Segmentation Fault (Segfault) Nedir?

Bir programın, kendisine ait olmayan veya erişim izni bulunmayan bir bellek bölgesine dokunmaya çalışmasıdır.

### Yaygın Sebepler:
- `NULL` pointer'ı dereference etmek (`*ptr` yaparken `ptr`'nin 0 olması).
- Dizinin sınırları dışına çıkmak (`dizi[100]` ama dizi 10 elemanlı).
- `free()` edilmiş bir yeri kullanmaya çalışmak (Use-after-free).
- Salt-okunur (ReadOnly) bir veriyi değiştirmeye çalışmak.

---

## 2. En Tehlikeli Üçlü: Leak, Double Free, Use-After-Free

### 2.1. Bellek Sızıntısı (Memory Leak)
`malloc` ile alınan yerin `free` edilmemesidir. Program çalıştığı sürece RAM dolar.
**Önlem:** Her `malloc` yazdığında hemen altına `free` satırını da ekle, sonra arayı doldur.

### 2.2. Double Free (Çift Tahliye)
Zaten `free` edilmiş bir pointer'ı tekrar `free` etmeye çalışmaktır. Programı anında çökertir.
**Önlem:** `free(p); p = NULL;` kuralını uygula. `NULL` bir pointer'ı tekrar `free` etmek C'de güvenlidir (hiçbir şey yapmaz).

### 2.3. Use-After-Free (Tahliye Sonrası Kullanım)
`free` edilmiş adresi hala kullanmaya çalışmaktır. Bu çok sinsi bir hatadır çünkü bazen çalışır, bazen çalışmaz. Siber saldırganlar bu hatayı kullanarak programın akışını değiştirebilir.

---

## 3. Buffer Overflow (Tampon Taşması)

En meşhur güvenlik açığıdır.
```c
char sifre[8];
gets(sifre); // Kullanıcı 20 karakter girerse...
```
8 karakterlik yere 20 karakter girilince, kalan 12 karakter bellekteki "Dönüş Adresi" (Return Address) gibi kritik verilerin üstüne yazar. Saldırgan buraya kendi kodunun adresini koyarak bilgisayarınızı ele geçirebilir.

---

## 4. Öğrenme Köşesi: "Off-by-One" Hataları

Döngülerde `<=` yerine `<` kullanmak gibi küçük hatalar, bellekte tam 1 byte'lık taşma yapar. Bu 1 byte, yanındaki değişkenin değerini değiştirerek saatlerce sürecek bir hata ayıklama (debugging) sürecine neden olabilir.

---

## 5. StackOverflow Notları: "Scanf" Güvenliği

`scanf("%s", buffer);` yazmak, `gets()` kadar tehlikelidir. Her zaman uzunluk belirtin:
```c
scanf("%19s", buffer); // En fazla 19 karakter oku (+1 null terminator).
```

---

## 6. Gerçek Hayat: Address Sanitizer (ASan)

Modern derleyicilerde (GCC ve Clang) müthiş bir özellik vardır: **Address Sanitizer**.
Derleme sırasında şu bayrağı eklerseniz:
```bash
gcc -fsanitize=address -g main.c -o program
```
Programınız bir bellek hatası yaptığı anda, nerede ne olduğunu kırmızı yazılarla detaylıca açıklar ve programı güvenle durdurur.

---

*Programı parçalara bölmek:* [03-moduler-programlama/01-fonksiyonlar-ve-stack-frame.md](../03-moduler-programlama/01-fonksiyonlar-ve-stack-frame.md)
