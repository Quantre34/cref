---
baslik: "stdatomic.h - Atomik İşlemler ve Thread Güvenliği"
kategori: "ileri"
etiketler: ["atomic", "thread", "stdatomic", "concurrency", "mutex"]
zorluk: "ileri"
ilgili: ["01-fonksiyon-pointerlari.md", "07-ileri-seviye-c/03-inline-ve-register.md"]
---

# stdatomic.h - Atomik İşlemler ve Thread Güvenliği

Çok çekirdekli işlemcilerde, birden fazla "thread" aynı değişkene aynı anda dokunmaya çalışırsa (Race Condition), veriler bozulur. Atomik işlemler, bu sorunu donanım seviyesinde çözer.

## 1. Atomik İşlem Ne Demektir?

**Atomik** (parçalanamaz) işlem, bir işlemci komutunun ya tamamen yapılması ya da hiç yapılmamasıdır. Arada başka bir thread araya giremez.

**Analoji: Bankamatik**
Hesabınızdan para çekerken; "bakiyeyi oku", "parayı düş", "yeni bakiyeyi yaz" işlemleri atomik olmalıdır. Eğer atomik olmazsa, siz tam parayı düşerken banka da faiz işletmeye çalışırsa bakiyeniz hatalı kaydedilebilir.

---

## 2. Atomik Değişken Tanımlama (C11)

C11 standardıyla gelen `_Atomic` anahtar kelimesi kullanılır.

```c
#include <stdatomic.h>

atomic_int sayac = 0; // Bu değişken artık thread-safe (güvenli)
```

---

## 3. Temel Atomik Fonksiyonlar

Normal `x = x + 1` işlemi atomik değildir (Oku -> Artır -> Yaz). Onun yerine şu fonksiyonlar kullanılır:

| Fonksiyon | Görevi |
|-----------|--------|
| `atomic_fetch_add(ptr, v)` | Değeri `v` kadar artır ve eski değeri döndür. |
| `atomic_fetch_sub(ptr, v)` | Değeri `v` kadar azalt. |
| `atomic_store(ptr, v)` | Değeri güvenli bir şekilde yaz. |
| `atomic_load(ptr)` | Değeri güvenli bir şekilde oku. |
| `atomic_exchange(ptr, v)` | Değeri `v` yap ve eski değeri al. |

---

## 4. CAS (Compare and Swap) Mekanizması

Atomik işlemlerin kalbi `atomic_compare_exchange` fonksiyonudur.
"Eğer değer beklediğim sayıysa, onu şu sayı yap" demektir. Bu, kilit (lock) kullanmadan güvenli yapılar kurmayı sağlar (**Lock-free programming**).

---

## 5. Öğrenme Köşesi: Neden Mutex Yerine Atomic?

- **Mutex (Kilit):** Diğer thread'leri "bekletir" (uyutur). Bu bir maliyettir.
- **Atomic:** Bekletmez, doğrudan işlemci üzerindeki özel bir sinyalle (lock prefix) işlemi bitirir. Çok daha hızlıdır ama sadece tekil değişkenler için uygundur.

---

## 6. StackOverflow Notları: "Memory Ordering"

Atomik işlemlerde `memory_order_relaxed` gibi kavramlar görürsünüz. Bu, işlemcinin ve derleyicinin performansı artırmak için kodların sırasını değiştirmesine (reordering) izin verip vermeyeceğinizi söyler. Çok ileri seviye bir konudur; varsayılan olarak `memory_order_seq_cst` (en güvenli olan) kullanılır.

---

## 7. Gerçek Hayat: Yüksek Performanslı Sunucular

Nginx veya Redis gibi saniyede milyonlarca istek alan yazılımlar, her istekte bir kilit (mutex) kullanırlarsa yavaşlarlar. Bunun yerine sayaçlar ve durum bayrakları için atomik işlemleri tercih ederler.

---

*Çok kanallı programlamaya giriş:* [threads-detay.md](threads-detay.md)
