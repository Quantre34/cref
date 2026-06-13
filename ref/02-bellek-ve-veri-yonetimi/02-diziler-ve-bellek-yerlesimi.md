---
baslik: "Diziler ve Bellek Yerleşimi"
kategori: "bellek"
etiketler: ["dizi", "array", "bellek", "contiguous", "index"]
zorluk: "orta"
ilgili: ["01-pointer-felsefesi.md", "03-string-manipulasyonu.md"]
---

# Diziler ve Bellek Yerleşimi

Diziler, aynı tipteki verilerin bellekte **yan yana** (sıralı) dizilmesidir. C'de bir dizi aslında sadece ilk elemanın adresini temsil eder.

## 1. Bellekteki Görüntü (Under the Hood)

```c
int dizi[3] = {10, 20, 30};
```
Bellekte şöyle bir manzara oluşur (Her `int` 4 byte varsayarsak):
- `dizi[0]` -> Adres: 1000, Değer: 10
- `dizi[1]` -> Adres: 1004, Değer: 20
- `dizi[2]` -> Adres: 1008, Değer: 30

**Kritik Bilgi:** `dizi` ismi, aslında `&dizi[0]` (yani 1000) adresine sabitlenmiş bir pointer gibidir.

---

## 2. Diziler ve Pointer İlişkisi (Syntactic Sugar)

C'de `dizi[i]` yazımı aslında arka planda şuna dönüşür:
```c
*(dizi + i)
```
- `dizi` adresine git.
- Üzerine `i` tane eleman boyutu ekle.
- O adresteki değeri getir.

**Deney:** `2[dizi]` yazarsanız da çalışır! Çünkü `*(2 + dizi)` ile `*(dizi + 2)` aynı şeydir. (Ama bunu kodunuzda yapmayın, arkadaşlarınız sizi sevmez).

---

## 3. Dizilerin Fonksiyonlara Gönderilmesi (Decay)

Bir diziyi fonksiyona gönderdiğinizde, dizinin tamamı kopyalanmaz. Sadece ilk elemanın adresi (pointer) kopyalanır. Buna **Pointer Decay** denir.

```c
void yazdir(int *p, int boyut) {
    // p burada sadece bir adrestir, dizinin boyutunu bilmez!
}
```
Bu yüzden fonksiyona her zaman dizinin boyutunu da ayrıca göndermelisiniz.

---

## 4. Çok Boyutlu Diziler (2D Arrays)

Bellek aslında tek boyutludur. 2D diziler (matrisler) bellekte "satır satır" (row-major order) dümdüz dizilir.

```c
int matris[2][2] = {{1, 2}, {3, 4}};
// Bellekte: 1, 2, 3, 4
```

---

## 5. Öğrenme Köşesi: VLA (Variable Length Array)

C99 ile gelen bu özellik, boyutu çalışma zamanında belirlenen dizilere izin verir:
```c
int n;
scanf("%d", &n);
int dizi[n]; // VLA
```
**Dikkat:** VLA'lar **Stack** üzerinde yer açar. Eğer `n` çok büyükse (örneğin 1 milyon), programınız anında çöker (**Stack Overflow**). Büyük veriler için her zaman Dinamik Bellek kullanın.

---

## 6. StackOverflow Notları: "Array vs Pointer" Farkı

- `int dizi[10]` -> Bellekte 40 byte yer ayırır. `dizi` isminin adresi değiştirilemez (sabit pointer).
- `int *ptr` -> Bellekte 8 byte yer ayırır (adres tutmak için). `ptr` başka adresleri gösterecek şekilde değiştirilebilir.

---

## 7. Gerçek Hayat: Cache Locality (Önbellek Verimliliği)

Diziler bellekte yan yana olduğu için işlemci (CPU) onları çok hızlı okur. Bir elemanı okuduğunda, bir sonrakini de "ihtiyacı olur" diye önbelleğe (Cache) çeker. Bağlı listeler gibi dağınık yapılar yerine dizi kullanmak, programınızı 10-20 kat hızlandırabilir.

---

*Özel bir dizi türü: Metinler:* [03-string-manipulasyonu.md](03-string-manipulasyonu.md)
