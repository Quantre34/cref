---
baslik: "Dinamik Bellek Yönetimi (Heap)"
kategori: "bellek"
etiketler: ["malloc", "free", "heap", "stack", "calloc", "realloc"]
zorluk: "ileri"
ilgili: ["01-pointer-felsefesi.md", "05-bellek-hatalari-ve-guvenlik.md"]
---

# Dinamik Bellek Yönetimi (Heap)

Şu ana kadar gördüğümüz tüm değişkenler **Stack** (Yığın) üzerindeydi. Stack otomatiktir ama kısıtlıdır. Büyük ve boyutu belirsiz veriler için **Heap** (Öbek) denilen geniş alanı kullanırız.

## 1. Stack vs Heap (Mental Model)

| Özellik | Stack (Yığın) | Heap (Öbek) |
|---------|---------------|-------------|
| **Analoji** | Küçük bir mutfak tezgahı. | Dev bir depo. |
| **Yönetim** | Otomatik (Kendi temizlenir). | Manuel (Sen temizlersin). |
| **Hız** | Çok hızlı. | Biraz daha yavaş. |
| **Boyut** | Küçük (Örn: 8 MB). | Çok büyük (Tüm RAM). |

---

## 2. Temel Fonksiyonlar (`stdlib.h`)

Heap'ten yer kiralamak için dört ana fonksiyonumuz vardır:

### 2.1. malloc (Memory Allocation)
- **Açılım:** **M**emory **Alloc**ation (Bellek Tahsisi).
- **İmza:** `void *malloc(size_t size)`
    - **Girdi (`size`):** Ayrılacak toplam byte sayısı. `size_t` türündedir (işaretsiz tam sayı).
    - **Çıktı (`void *`):** Ayrılan yerin başlangıç adresi. `void *` olduğu için her türden pointer'a (int, struct vb.) atanabilir.
- **Amaç:** Programın çalışma zamanında (runtime), derleyicinin önceden bilmediği miktardaki veriyi depolamak için RAM'den yer kiralamaktır.
```c
int *p = malloc(10 * sizeof(int)); // 10 int'lik (genelde 40 byte) yer kirala.
```
- **İşleyiş:** Sadece yer ayırır, içini temizlemez (çöp değerler kalır). Eğer sistemde yeterli yer yoksa `NULL` döner. **Mutlaka** dönen değeri kontrol etmelisiniz.

### 2.2. calloc (Clean Allocation)
```c
int *p = calloc(10, sizeof(int));
```
- **İşleyiş:** Yer ayırır ve tüm bitleri `0` yapar. Biraz daha yavaştır ama güvenlidir.

### 2.3. realloc (Re-allocation)
```c
p = realloc(p, 20 * sizeof(int)); // Mevcut yeri büyüt veya küçült.
```

### 2.4. free (De-allocation)
```c
free(p); // İşim bitti, depoyu boşalt.
```

---

## 3. Altın Kural: Kiraladığını Geri Ver!

C'de çöp toplayıcı (Garbage Collector) yoktur. `malloc` ile aldığınız her byte'ı, `free` ile geri vermelisiniz.

- **Bellek Sızıntısı (Memory Leak):** Eğer `free` yapmayı unutursanız, programınız RAM'i yemeye devam eder ve sonunda bilgisayar kilitlenir.

---

## 4. Öğrenme Köşesi: "Dangling Pointer" (Sarkan İşaretçi)

```c
free(p);
*p = 10; // FACİA!
```
`free(p)` yaptıktan sonra `p` hala o adresi göstermeye devam eder ama o adres artık sizin değildir (Kiralık evi boşalttınız ama anahtarı hala sizde). O eve girmeye çalışırsanız "Haneye Tecavüz"den (Segfault) ceza alırsınız.
- **Çözüm:** `free(p); p = NULL;` yaparak anahtarı imha edin.

---

## 5. StackOverflow Notları: `sizeof` Kullanımı

Asla `malloc(40)` yazmayın. `int` her sistemde 4 byte olmayabilir. Her zaman `malloc(10 * sizeof(int))` yazın.

---

## 6. Gerçek Hayat: Valgrind

Büyük projelerde binlerce `malloc` ve `free` vardır. Hangisini unuttuğunuzu bulmak imkansızdır. Bu yüzden **Valgrind** gibi araçlar kullanılır.
```bash
valgrind --leak-check=full ./program
```
Bu araç, hangi satırda `free` yapmayı unuttuğunuzu size tek tek söyler.

---

*Hatalardan nasıl korunuruz?* [05-bellek-hatalari-ve-guvenlik.md](05-bellek-hatalari-ve-guvenlik.md)
