---
baslik: "Değişkenler ve Bellek İlişkisi"
kategori: "temel"
etiketler: ["degisken", "bellek", "scope", "hizalama", "adlandirma"]
zorluk: "baslangic"
ilgili: ["01-program-yapisi.md", "03-veri-tipleri-derinlemesine.md"]
---

# Değişkenler ve Bellek İlişkisi

C'de bir değişken tanımladığınızda, aslında bilgisayarın RAM'inden (rastgele erişimli bellek) bir parça yer kiralarsınız.

## 1. Değişken Nedir? (Mental Model)

Bir değişkeni bir **kutu** olarak düşünün.
- **Kutunun Adı:** Değişkenin ismi (etiket).
- **Kutunun Boyutu:** Veri tipi (kaç byte kapladığı).
- **Kutunun İçeriği:** Atanan değer.
- **Kutunun Konumu:** Bellek adresi (RAM üzerindeki numarası).

```c
int yas = 25;
```
Bu satırı yazdığınızda:
1. İşletim sistemi RAM'den 4 byte'lık bir yer ayırır.
2. Bu yere `yas` etiketini yapıştırır.
3. İçine `25` sayısını (ikilik sistemde) koyar.

---

## 2. Adlandırma Sanatı (Naming Conventions)

C'de değişken isimleri rastgele olamaz. Bazı katı kurallar ve topluluk alışkanlıkları vardır.

### Kurallar:
- Harf veya alt çizgi (`_`) ile başlamalıdır.
- Rakamla başlayamaz (`2sayi` HATALI).
- Boşluk veya özel karakter (`-`, `!`, `@`) içeremez.
- C'nin anahtar kelimeleri (`int`, `while`, `if` vb.) isim olamaz.

### Stil Önerileri:
- **snake_case:** `kullanici_yasi` (C topluluğunda en yaygın stil).
- **camelCase:** `kullaniciYasi` (Daha çok Java/JS dünyasında yaygın).

---

## 3. Scope: Değişkenin Ömrü Nerede Biter?

Her değişkenin bir "yaşam alanı" vardır. Buna **Scope** denir.

```c
#include <stdio.h>

int global_x = 10; // 1. Global: Her yerden erişilir. Program bitene kadar ölmez.

int main(void) {
    int yerel_y = 20; // 2. Yerel (Local): Sadece main içinde yaşar. main bitince silinir.
    
    {
        int blok_z = 30; // 3. Blok: Sadece bu süslü parantez içinde yaşar.
        printf("%d", yerel_y); // Çalışır.
    }
    
    // printf("%d", blok_z); // HATA! blok_z artık yok (Scope dışı).
    return 0;
}
```

---

## 4. İşleyiş: Başlatılmamış Değişkenler (Uninitialized)

C'de bir değişkeni tanımlayıp değer vermezseniz, içinden ne çıkacağını bilemezsiniz.

```c
int x; 
printf("%d", x); // Çıktı: 32767 veya -123123 veya 0...
```
- **Neden?** RAM'den o bölge size tahsis edildiğinde, orada daha önceden kalan "çöp veriler" (garbage values) olabilir. C bunları temizlemekle vakit kaybetmez. Temizlik sizin sorumluluğunuzdadır.

---

## 5. Öğrenme Köşesi: "L-Value" ve "R-Value" Nedir?

```c
x = 5;
```
- **L-Value (Left):** Eşittir'in solundaki. Bellekte bir adresi olan "kutunun kendisidir".
- **R-Value (Right):** Eşittir'in sağındaki. Kutunun içine konan "değerdir".

Eğer `5 = x;` yazarsanız hata alırsınız; çünkü `5` bir sabit değerdir, bellekte bir kutusu (adresi) yoktur, dolayısıyla içine bir şey konulamaz.

---

## 6. StackOverflow Notları: "Magic Numbers"

Kodunuzun ortasında `x = 86400 * gun;` gibi sayılar kullanmayın. `86400` nedir?
Bunun yerine:
```c
const int SANIYE_BIR_GUN = 86400;
x = SANIYE_BIR_GUN * gun;
```
Bu, kodunuzun okunabilirliğini artırır ve hatayı önler.

---

## 7. Gerçek Hayat: RAM Verimliliği

Gömülü sistemlerde (mikrodenetleyiciler) RAM çok azdır (birkaç KB). Gereksiz her büyük değişken, sistemin kilitlenmesine yol açabilir. Bu yüzden doğru veri tipini seçmek hayati önem taşır.

---

*Hangi kutu ne kadar yer kaplar?* [03-veri-tipleri-derinlemesine.md](03-veri-tipleri-derinlemesine.md)
