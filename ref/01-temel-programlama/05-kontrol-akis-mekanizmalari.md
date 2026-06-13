---
baslik: "Kontrol Akış Mekanizmaları"
kategori: "temel"
etiketler: ["if", "else", "switch", "case", "ternary", "branching"]
zorluk: "baslangic"
ilgili: ["04-operatorler-ve-oncelik.md", "06-dongu-stratejileri.md"]
---

# Kontrol Akış Mekanizmaları

Programlar her zaman yukarıdan aşağıya dümdüz akmaz. Karar vermeleri gerekir. C'de bu "yol ayrımı" işlemini kontrol yapıları yapar.

## 1. if, else if, else (Dinamik Kararlar)

En temel karar yapısıdır.

```c
if (sicaklik > 30) {
    printf("Hava sıcak.");
} else if (sicaklik > 15) {
    printf("Hava ideal.");
} else {
    printf("Hava soğuk.");
}
```

### İşleyiş: Dallanma (Branching)
İşlemci (CPU) bir `if` gördüğünde, sonucun ne olacağını tahmin etmeye çalışır (**Branch Prediction**). Eğer yanlış tahmin ederse, o ana kadar yaptığı işleri çöpe atıp diğer yola girmek zorunda kalır. Bu yüzden çok karmaşık ve iç içe `if` yapıları programı bir miktar yavaşlatabilir.

---

## 2. switch-case (Çoklu Seçenekler)

Bir değişkenin birçok farklı tam sayı (veya karakter) değerini kontrol ediyorsanız, `switch` daha temizdir.

```c
switch (gun) {
    case 1: printf("Pazartesi"); break;
    case 2: printf("Salı"); break;
    // ...
    default: printf("Geçersiz gün");
}
```

### Kritik: `break` Unutmak!
Eğer `break` koymazsanız, kod bir sonraki `case`'e "akar" (Fall-through). Bu bazen istenen bir şeydir (örneğin hem 1 hem 2 için aynı işi yapmak), ama genelde hataya yol açar.

---

## 3. Ternary Operator (Üçlü Operatör)

Basit `if-else` yapılarını tek satıra indirir.

```c
// Klasik
if (a > b) max = a; else max = b;

// Ternary
max = (a > b) ? a : b;
```

**Analoji:** "Hava yağmurlu mu? Evet ise şemsiye al : Hayır ise güneş gözlüğü al."

---

## 4. Öğrenme Köşesi: Dangling Else (Asılı Kalan Else)

```c
if (x > 0)
    if (y > 0)
        printf("Pozitif");
else
    printf("Negatif?"); // Bu else hangi if'e ait?
```
C kuralına göre `else` her zaman en yakınındaki (yukarıdaki) `if`'e aittir. Karışıklığı önlemek için **HER ZAMAN** süslü parantez `{}` kullanın.

---

## 5. StackOverflow Notları: `if (x = 5)` Hatası

```c
if (x = 5) { ... }
```
Bu bir karşılaştırma (`==`) değil, bir atamadır (`=`). `x`'e 5 atanır ve 5 sıfır olmadığı için `if` her zaman DOĞRU döner.
- **Profesyonel İpucu:** Bazı yazılımcılar `if (5 == x)` (Yoda Conditions) yazar. Çünkü yanlışlıkla `if (5 = x)` yazarsanız derleyici hata verir.

---

## 6. Gerçek Hayat: State Machines (Durum Makineleri)

`switch-case` yapıları, oyun motorlarında veya gömülü sistemlerde "Durum Makinesi" (Finite State Machine) kurmak için kullanılır.
- `DURUM_BEKLEME`, `DURUM_YURU`, `DURUM_SALDIR` gibi modlar arasında geçiş bu yapıyla kontrol edilir.

---

*Tekrar eden işlerin çözümü:* [06-dongu-stratejileri.md](06-dongu-stratejileri.md)
