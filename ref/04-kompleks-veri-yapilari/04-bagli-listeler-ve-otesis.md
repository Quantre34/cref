---
baslik: "Bağlı Listeler ve Veri Yapıları"
kategori: "veri-yapilari"
etiketler: ["linked-list", "bagli-liste", "dugum", "node", "stack", "queue"]
zorluk: "ileri"
ilgili: ["01-struct-ve-bellek-hizalama.md", "04-dinamik-bellek-yonetimi.md"]
---

# Bağlı Listeler ve Veri Yapıları

Diziler bellekte yan yana dizilir ve boyutları sabittir. Eğer çalışma zamanında sürekli büyüyen veya küçülen bir veriye ihtiyacınız varsa, **Bağlı Liste (Linked List)** kullanmalısınız.

## 1. Bağlı Liste Nedir? (Mental Model)

Bağlı listeyi bir **hazine avı** gibi düşünün.
- Her istasyonda bir hazine (veri) ve bir sonraki istasyonun yerini söyleyen bir harita (pointer) vardır.
- Bir sonraki istasyonu bilmek için mevcut istasyonda olmanız gerekir.
- Son istasyondaki harita `NULL`'u gösterir.

```c
struct Dugum {
    int veri;
    struct Dugum *sonraki;
};
```

---

## 2. Diziler vs Bağlı Listeler

| Özellik | Dizi | Bağlı Liste |
|---------|------|-------------|
| **Erişim** | Çok hızlı (`dizi[5]`). | Yavaş (Baştan saymak gerek). |
| **Ekleme** | Yavaş (Kaydırmak gerek). | Çok hızlı (Sadece linki değiştir). |
| **Bellek** | Yan yana (Contiguous). | Dağınık (Non-contiguous). |
| **Boyut** | Sabit. | Dinamik. |

---

## 3. Temel İşlemler

1. **Ekleme:** Yeni bir `malloc` ile düğüm oluştur ve bir önceki düğümün `sonraki` pointer'ını buraya bağla.
2. **Silme:** Silinecek düğümü atlayıp bir öncekini bir sonrakine bağla ve silineni `free()` et.
3. **Gezme:** `while(temp != NULL)` döngüsüyle sonuna kadar git.

---

## 4. Diğer Veri Yapılarına Giriş

Bağlı listeleri ve dizileri kullanarak daha karmaşık yapılar kurabiliriz:

- **Stack (Yığın):** LIFO (Son giren ilk çıkar). Örn: Tarayıcıdaki "Geri" butonu.
- **Queue (Kuyruk):** FIFO (İlk giren ilk çıkar). Örn: Yazıcı kuyruğu.
- **Tree (Ağaç):** Hiyerarşik veri. Örn: Bilgisayarınızdaki klasör yapısı.

---

## 5. Öğrenme Köşesi: "Dangling Pointer" Tehlikesi

Bağlı listeden bir düğüm silerken sırayı karıştırmayın!
```c
// HATALI
free(silinecek);
temp->sonraki = silinecek->sonraki; // ERROR! Sildiğin düğümün içindeki veriye artık bakamazsın.
```

---

## 6. StackOverflow Notları: "Sentinel Nodes"

Listenin başına ve sonuna boş "bekçi" (sentinel) düğümler koymak, "liste boş mu?", "başından mı siliyorum?" gibi özel durum kontrollerini (if-else) azaltır ve kodunuzu çok daha temiz hale getirir.

---

## 7. Gerçek Hayat: Linux Çekirdeği

Linux çekirdeğinin (kernel) her yerinde bağlı listeler kullanılır. İşlem listesi, açık dosyalar, ağ paketleri... C'de veri yapılarını anlamak, işletim sistemlerinin nasıl çalıştığını anlamaktır.

---

*Dosyalarla kalıcı veri saklamak:* [05-dosya-sistemleri-ve-io/01-standart-io-akislari.md](../05-dosya-sistemleri-ve-io/01-standart-io-akislari.md)
