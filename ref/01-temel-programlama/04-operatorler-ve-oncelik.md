---
baslik: "Operatörler ve İşlem Önceliği"
kategori: "temel"
etiketler: ["operator", "aritmetik", "mantiksal", "oncelik", "karsilastirma"]
zorluk: "baslangic"
ilgili: ["03-veri-tipleri-derinlemesine.md", "15-bit-islemleri.md"]
---

# Operatörler ve İşlem Önceliği

Operatörler, veriler üzerinde işlem yapan sembollerdir. C'de her operatörün bir "gücü" (önceliği) vardır.

## 1. Aritmetik Operatörler

| Operatör | İşlem | Not |
|----------|-------|-----|
| `+` | Toplama | |
| `-` | Çıkarma | |
| `*` | Çarpma | |
| `/` | Bölme | Tam sayılarda ondalığı atar! |
| `%` | Modulo | Kalanı verir (sadece tam sayılarda). |

### Kritik: Tam Sayı Bölmesi
```c
float sonuc = 5 / 2;
printf("%f", sonuc); // Çıktı: 2.000000 (2.5 değil!)
```
- **Neden?** İki tam sayının işlemi her zaman tam sayıdır. 2.5 olur ama tam sayı olduğu için .5 atılır.
- **Çözüm:** `5.0 / 2` veya `(float)5 / 2` (Casting).

---

## 2. Mantıksal ve Karşılaştırma

C'de `true` veya `false` tipi (C99 öncesi) yoktur. `0` yanlıştır, **0 dışındaki her şey** doğrudur.

| Operatör | Anlamı |
|----------|--------|
| `==` | Eşit mi? |
| `!=` | Eşit değil mi? |
| `&&` | VE (AND) |
| `||` | VEYA (OR) |
| `!` | DEĞİL (NOT) |

---

## 3. Artırma ve Azaltma (++/--)

- `x++`: Önce kullan, sonra artır.
- `++x`: Önce artır, sonra kullan.

```c
int a = 5, b;
b = a++; // b=5, a=6
b = ++a; // a=7, b=7
```

---

## 4. İşlem Önceliği (Precedence)

Matematikteki çarpma-bölme önceliği C'de de geçerlidir ama liste çok daha uzundur.

| Öncelik | Operatör Grubu |
|---------|----------------|
| 1 | `()` `[]` `->` `.` |
| 2 | `++` `--` `!` `&` `*` (Unary) |
| 3 | `*` `/` `%` |
| 4 | `+` `-` |
| 5 | `<` `>` `<=` `>=` |
| 6 | `==` `!=` |
| 7 | `&&` |
| 8 | `||` |
| 9 | `=` `+=` `-=` vb. |

**Altın Kural:** Şüpheye düştüğünde parantez `()` kullan! Parantez bedavadır, hata paha biçilemez.

---

## 5. Öğrenme Köşesi: "Short-Circuit" (Kısa Devre)

Mantıksal işlemlerde C, sonucun belli olduğu noktada durur.

```c
if (a == 0 || b / a > 1) { ... }
```
Eğer `a` sıfırsa, C `||` operatörünün sol tarafının doğru olduğunu görür ve sağ tarafı (bölme işlemini) hiç çalıştırmaz. Bu sayede "sıfıra bölme" hatasından kurtulursunuz.

---

## 6. StackOverflow Notları: `x = x++` Bilmecesi

```c
int x = 5;
x = x++;
```
Bunun sonucu nedir? C standardına göre bu **Undefined Behavior** (Tanımsız Davranış) dır. Bazı derleyiciler 5, bazıları 6 verebilir. Bir değişkeni aynı satırda hem değiştirip hem atama yapmayın.

---

## 7. Gerçek Hayat: Bitmasking

Operatörler sadece matematik yapmaz. `&` (Bitwise AND) ve `|` (Bitwise OR) operatörleri, donanım register'larındaki tek bir biti açıp kapatmak için kullanılır.

*Koşullara göre yol ayrımı:* [05-kontrol-akis-mekanizmalari.md](05-kontrol-akis-mekanizmalari.md)
