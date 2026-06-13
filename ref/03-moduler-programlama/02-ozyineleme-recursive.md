---
baslik: "Özyineleme (Recursion)"
kategori: "moduler"
etiketler: ["recursion", "ozyineleme", "stack", "base-case", "faktoriyel"]
zorluk: "orta"
ilgili: ["01-fonksiyonlar-ve-stack-frame.md", "20-veri-yapilari.md"]
---

# Özyineleme (Recursion)

Özyineleme, bir fonksiyonun bir problemi çözmek için kendisini daha küçük parçalarla tekrar çağırmasıdır.

## 1. Temel Kurallar (Mental Model)

Bir özyinelemeli fonksiyonun iki hayati parçası vardır:
1. **Durma Koşulu (Base Case):** Fonksiyonun kendini çağırmayı bıraktığı andır. Bu olmazsa sonsuza kadar döner.
2. **Özyinelemeli Adım (Recursive Step):** Problemi biraz daha küçülterek kendini çağırdığı andır.

**Analoji: Matruşka Bebekleri**
En dıştaki bebeği açarsınız (Fonksiyon çağrısı). İçinden daha küçük bir bebek çıkar (Aynı fonksiyon, daha küçük veri). En sondaki en küçük bebeğe ulaşana kadar devam edersiniz (Base Case).

---

## 2. Klasik Örnek: Faktöriyel

```c
int faktoriyel(int n) {
    if (n <= 1) return 1;          // 1. Base Case
    return n * faktoriyel(n - 1);  // 2. Recursive Step
}
```

### Stack'te Neler Oluyor?
`faktoriyel(3)` çağrıldığında:
1. `faktoriyel(3)` bir frame açar, bekler.
2. `faktoriyel(2)` yeni bir frame açar, bekler.
3. `faktoriyel(1)` base case'e ulaşır, `1` döner.
4. Bekleyenler sırayla hesaplanır: `2 * 1 = 2`, sonra `3 * 2 = 6`.

---

## 3. Özyineleme vs Döngü

Her özyinelemeli çözüm bir döngüyle (`for`/`while`) yazılabilir.

| Özellik | Özyineleme | Döngü |
|---------|------------|-------|
| **Okunabilirlik** | Çok yüksek (Matematiksel). | Daha karmaşık olabilir. |
| **Performans** | Stack yükü bindirir. | Daha hızlıdır. |
| **Bellek** | Çok yer kaplar. | Az yer kaplar. |

---

## 4. Öğrenme Köşesi: "Infinite Recursion" (Sonsuz Özyineleme)

Durma koşulunu unutursanız ne olur?
```c
void sonsuz() {
    sonsuz();
}
```
Her çağrı yeni bir **Stack Frame** açar. Sonunda Stack dolar ve işletim sistemi "Dur artık!" diyerek programı çökertir. Buna **Stack Overflow** denir.

---

## 5. StackOverflow Notları: Tail Call Optimization

Bazı modern derleyiciler, eğer fonksiyonun en son satırı sadece kendini çağırmaksa (Tail Recursion), yeni bir frame açmak yerine mevcut frame'i tekrar kullanır. Bu, özyinelemeyi döngü kadar hızlı yapar. Ancak C'de buna her zaman güvenemezsiniz.

---

## 6. Gerçek Hayat: Dosya Sistemi Gezme

Bilgisayarınızdaki klasörlerin içinde klasörler vardır. Bir klasörün boyutunu hesaplamak için en doğal yol özyinelemedir:
- Klasörün içindeki dosyaları topla.
- Eğer içinde başka klasör varsa, o klasör için de aynı fonksiyonu çağır.

---

*Projeyi dosyalara bölmek:* [03-header-dosyalari-ve-kapsam.md](03-header-dosyalari-ve-kapsam.md)
