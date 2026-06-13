---
baslik: "Hash Maps ve Hızlı Arama"
kategori: "algoritmalar"
etiketler: ["hash", "map", "dictionary", "key-value", "o1"]
zorluk: "ileri"
ilgili: ["04-kompleks-veri-yapilari/04-bagli-listeler-ve-otesis.md"]
---

# Hash Maps ve Hızlı Arama

C'de standart bir Hash Map (Sözlük) yapısı yoktur; ancak sistem programlamada en çok kullanılan veri yapılarından biridir. Amacı, bir "anahtar" (key) ile bir "değer"e (value) **O(1)** hızında ulaşmaktır.

## 1. Hash Fonksiyonu Nedir?

Hash fonksiyonu, herhangi bir uzunluktaki metni veya veriyi, sabit uzunlukta bir sayıya (index) çeviren matematiksel bir işlemdir.

**Analoji:** Kütüphanedeki kitapları, isimlerinin baş harflerine göre raflara dizmek. "A" ile başlayan kitabı bulmak için tüm kütüphaneyi gezmezsiniz, sadece "A" rafına bakarsınız.

---

## 2. Çakışma (Collision) Yönetimi

İki farklı metin aynı sayıya (index'e) çıkarsa ne olur?
- **Chaining (Zincirleme):** O index'te bir bağlı liste (linked list) tutulur.
- **Open Addressing:** Boş olan bir sonraki index'e gidilir.

---

## 3. Basit Bir Hash Fonksiyonu Örneği (DJB2)

```c
unsigned long hash(unsigned char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash;
}
```

---

## 4. Öğrenme Köşesi: Zaman Karmaşıklığı

- **Dizi Arama:** O(n) - Tek tek bakmak gerekir.
- **Sıralı Dizi (Binary Search):** O(log n).
- **Hash Map:** O(1) - Tek hamlede bulur.

---

## 5. Gerçek Hayat: Veritabanı Indeksleri

Tüm veritabanları (MySQL, PostgreSQL vb.) ve önbellek sistemleri (Redis), veriyi milyonlarca satır arasından milisaniyeler içinde bulmak için Hash Map ve Ağaç (Tree) yapılarını kullanır.

---

*Hiyerarşik veri saklama:* [02-red-black-trees.md](02-red-black-trees.md)
