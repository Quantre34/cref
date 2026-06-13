---
baslik: "Red-Black Trees ve Kendini Dengeleyen Ağaçlar"
kategori: "algoritmalar"
etiketler: ["tree", "red-black", "balanced-tree", "search", "complexity"]
zorluk: "ileri"
ilgili: ["04-kompleks-veri-yapilari/04-bagli-listeler-ve-otesis.md"]
---

# Red-Black Trees ve Kendini Dengeleyen Ağaçlar

Normal ikili arama ağaçları (Binary Search Tree), veriler sıralı gelirse bir "bağlı liste"ye dönüşüp yavaşlayabilir. Red-Black ağaçları, kendilerini otomatik olarak dengede tutarak her zaman hızlı kalırlar.

## 1. Ağaç Neden Dengesizleşir?

Eğer bir ağaca sırayla `1, 2, 3, 4, 5` eklerseniz, ağaç sağa doğru uzayan bir çizgi olur. Arama hızı **O(n)** olur. Dengeleme, bu düğümleri sağa-sola dağıtarak boyu kısa tutar (**O(log n)**).

---

## 2. Red-Black Kuralları

1. Her düğüm ya **Kırmızı** ya da **Siyah**dır.
2. Kök her zaman **Siyah**dır.
3. Kırmızı bir düğümün çocukları **Siyah** olmalıdır (Üst üste iki kırmızı gelemez).
4. Bir düğümden yapraklara giden tüm yollarda aynı sayıda siyah düğüm olmalıdır.

---

## 3. Neden Red-Black?

- **Hız:** Arama, ekleme ve silme işlemleri her zaman dengelidir.
- **Kullanım:** C++ `std::map`, Java `TreeMap` ve Linux çekirdeği (CPU zamanlayıcısı) bu yapıyı kullanır.

---

## 4. Öğrenme Köşesi: Rotasyon (Döndürme)

Ağaç dengesi bozulduğunda, düğümleri "Sola Döndür" veya "Sağa Döndür" işlemleriyle yer değiştiririz. Bu, ağacın hiyerarşisini bozmadan boyunu kısaltır.

---

## 5. Gerçek Hayat: Linux CFS (Completely Fair Scheduler)

Linux çekirdeği, hangi programın işlemciyi ne kadar süre kullanacağını hesaplamak için bir Red-Black ağacı kullanır. En az süre kullanmış olan program ağacın en solundadır ve işlemciye ilk o girer.

---

*Hata yönetimi ve sinyaller:* [08-standart-kutuphane-referansi/signal-detay.md](../08-standart-kutuphane-referansi/signal-detay.md)
