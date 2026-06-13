---
baslik: "Döngü Stratejileri"
kategori: "temel"
etiketler: ["for", "while", "do-while", "break", "continue", "loops"]
zorluk: "baslangic"
ilgili: ["05-kontrol-akis-mekanizmalari.md", "07-diziler.md"]
---

# Döngü Stratejileri

Bilgisayarların en iyi yaptığı şey, sıkılmadan aynı şeyi milyonlarca kez tekrar etmektir. C'de bu işlemi `for`, `while` ve `do-while` ile yapıyoruz.

## 1. for Döngüsü (Sayılabilir Tekrar)

Eğer kaç kere döneceğinizi biliyorsanız (veya bir dizi üzerinde geziyorsanız) `for` en iyisidir.

```c
for (int i = 0; i < 10; i++) {
    printf("%d ", i);
}
```

### İşleyiş:
1. **Başlatma:** `int i = 0` (Sadece bir kez çalışır).
2. **Koşul:** `i < 10` (Her tur başında kontrol edilir).
3. **Gövde:** `{ ... }` (İşlem yapılır).
4. **Güncelleme:** `i++` (Her tur sonunda çalışır).

---

## 2. while Döngüsü (Belirsiz Tekrar)

Koşul doğru olduğu sürece döner. Kaç tur süreceği genelde bir girdiye veya dış etmene bağlıdır.

```c
while (oyun_devam_ediyor) {
    // İşlemler...
}
```

---

## 3. do-while (En Az Bir Kez)

Koşul ne olursa olsun, döngü gövdesi **en az bir kez** çalıştırılır. Sonra koşula bakılır.

```c
do {
    printf("Şifre girin: ");
    scanf("%d", &sifre);
} while (sifre != dogru_sifre);
```

---

## 4. Akış Kontrolü: break ve continue

- **break:** Döngüyü tamamen bitirir ve dışarı çıkar.
- **continue:** Mevcut turun geri kalanını atlar ve bir sonraki tura (güncelleme adımına) geçer.

---

## 5. Öğrenme Köşesi: Sonsuz Döngü Tehlikesi

```c
while (1) { ... }
// veya
for (;;) { ... }
```
Bu döngüler asla bitmez. Eğer içine bir `break` koymazsanız programınız "asılı kalır" ve CPU %100 kullanıma çıkar. 

---

## 6. StackOverflow Notları: Off-by-One Hatası

Programcıların en çok yaptığı hata: Döngüyü 1 eksik veya 1 fazla döndürmek.
```c
int dizi[10];
for (int i = 0; i <= 10; i++) { // HATA! i=10 olduğunda dizinin dışına taşar.
    dizi[i] = 0;
}
```
**Kural:** C'de indexler 0'dan başlar, bu yüzden 10 elemanlı bir dizi için son index 9'dur. Koşul `< 10` olmalıdır.

---

## 7. Gerçek Hayat: Loop Unrolling (Döngü Açma)

Çok hızlı çalışması gereken kodlarda derleyici, döngüdeki `i < 10` kontrolünü her seferinde yapmamak için kodu şu hale getirebilir:
```c
// i++ ve kontrol yerine:
islem(0);
islem(1);
islem(2);
// ...
```
Buna **Loop Unrolling** denir. Modern derleyiciler bunu sizin yerinize otomatik yapar.

---

*Tekrar eden verileri saklamak:* [02-bellek-ve-veri-yonetimi/02-diziler-ve-bellek-yerlesimi.md](../02-bellek-ve-veri-yonetimi/02-diziler-ve-bellek-yerlesimi.md)
