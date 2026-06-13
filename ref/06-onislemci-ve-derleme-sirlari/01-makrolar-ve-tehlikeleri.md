---
baslik: "Makrolar ve Tehlikeleri"
kategori: "derleme"
etiketler: ["define", "makro", "onislemci", "preprocessor", "inline"]
zorluk: "orta"
ilgili: ["02-kosullu-derleme.md", "03-derleme-asamalari-derinlemesine.md"]
---

# Makrolar ve Tehlikeleri

Ön işlemci (Preprocessor), derleme başlamadan önce kodunuzun üzerinde "bul ve değiştir" yapan basit bir metin düzenleyicidir. `#define` ile tanımlanan makrolar çok güçlüdür ama bir o kadar da sinsi hatalara yol açabilir.

## 1. Sabit Makrolar

```c
#define PI 3.14159
#define MAX_BUFFER 1024
```
**İşleyiş:** Derleyici kodda `PI` gördüğü her yere `3.14159` yazar. Bu bir değişken değildir, bellekte yer kaplamaz.

---

## 2. Fonksiyon Benzeri Makrolar

Küçük işlemler için fonksiyon çağırma yükünden (stack frame açmak) kurtulmak için kullanılırlar.

```c
#define KARE(x) x * x
```

### Büyük Tehlike: Öncelik Hatası
Eğer `KARE(2 + 2)` yazarsanız ne olur?
- Beklenen: 4 * 4 = 16
- Gerçekleşen: `2 + 2 * 2 + 2` = `2 + 4 + 2` = **8**!

**Altın Kural:** Makrolarda her şeyi parantez içine alın!
```c
#define KARE(x) ((x) * (x))
```

---

## 3. Makrolar vs Fonksiyonlar

| Özellik | Makro | Fonksiyon |
|---------|-------|-----------|
| **Tip Kontrolü** | Yok (Tehlikeli). | Var (Güvenli). |
| **Hız** | Daha hızlı (Kopyala-yapıştır). | Biraz daha yavaş (Call overhead). |
| **Hata Ayıklama** | Çok zor (Debugger satırı göremez). | Kolay. |
| **Kapsam** | Globaldir. | Local/Global olabilir. |

---

## 4. Öğrenme Köşesi: "Side Effects" (Yan Etkiler)

```c
#define MAX(a, b) ((a) > (b) ? (a) : (b))

int x = 5, y = 10;
int m = MAX(x++, y++);
```
Burada `y` değişkeni **iki kez** artar! Çünkü makro açıldığında `y++` ifadesi kodun içine iki kez yapıştırılır. Fonksiyonlarda bu asla olmaz.

---

## 5. StackOverflow Notları: "do while(0)" Tekniği

Çok satırlı makrolar yazarken `if-else` bloklarında hata almamak için şu yöntem kullanılır:
```c
#define LOG(msg) do { printf("LOG: "); printf(msg); } while(0)
```
Bu sayede makro her zaman tek bir ifade (statement) gibi davranır.

---

## 6. Gerçek Hayat: `inline` Fonksiyonlar

Modern C'de (C99+), makroların hızını ve fonksiyonların güvenliğini birleştiren `inline` anahtar kelimesi vardır.
```c
inline int kare(int x) { return x * x; }
```
Derleyiciye "Bu fonksiyonun kodunu, çağrıldığı yere doğrudan yapıştır (mümkünse)" demiş olursunuz. Makro yerine her zaman `inline` fonksiyonları tercih edin.

---

*Farklı sistemlere göre kod yazmak:* [02-kosullu-derleme.md](02-kosullu-derleme.md)
