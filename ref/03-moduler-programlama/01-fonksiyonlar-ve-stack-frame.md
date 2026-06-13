---
baslik: "Fonksiyonlar ve Stack Frame"
kategori: "moduler"
etiketler: ["fonksiyon", "stack-frame", "parametre", "return", "scope"]
zorluk: "orta"
ilgili: ["01-program-yapisi.md", "02-ozyineleme-recursive.md"]
---

# Fonksiyonlar ve Stack Frame

Fonksiyonlar, kodunuzun tekrar kullanılabilir yapı taşlarıdır. Ancak bir fonksiyonu çağırdığınızda arka planda çok karmaşık bir "bürokrasi" döner.

## 1. Fonksiyon Anatomisi

```c
int topla(int a, int b) { // İmzası (Signature)
    int sonuc = a + b;
    return sonuc;         // Dönüş Değeri
}
```

- **Dönüş Tipi (int):** Fonksiyon bittiğinde ne verecek? Hiçbir şey vermeyecekse `void`.
- **Parametreler (a, b):** Dışarıdan gelen veriler.
- **Scope:** Fonksiyon içindeki değişkenler (`sonuc`) dışarıdan görünmez.

---

## 2. Stack Frame (Under the Hood)

Bir fonksiyon çağrıldığında, belleğin **Stack** bölgesinde ona özel bir alan açılır. Buna **Stack Frame** denir.

### Bir Çağrı Sırasında Neler Olur?
1. **Argümanlar:** Fonksiyona gönderilen değerler stack'e itilir.
2. **Dönüş Adresi:** Fonksiyon bittiğinde kodun hangi satırına döneceği kaydedilir.
3. **Yerel Değişkenler:** Fonksiyonun içindeki değişkenler için yer açılır.
4. **İşlem:** Kod çalışır.
5. **Temizlik:** `return` dendiğinde tüm bu frame bellekten "pop" edilir (silinir).

**Kritik Bilgi:** Yerel değişkenlerin fonksiyon bitince "ölmesinin" sebebi, onlara ait olan Stack Frame'in yok edilmesidir.

---

## 3. Değerle mi, Adresle mi? (Call by Value vs Reference)

C'de her şey **Call by Value** (Değerle Çağırma) dır. Fonksiyona bir değişken gönderdiğinizde, onun bir kopyası oluşturulur.

```c
void artir(int x) { x++; } // Sadece kopyayı artırır!
```

Eğer orijinal değişkeni değiştirmek istiyorsanız, onun adresini (pointer) göndermelisiniz:
```c
void gercek_artir(int *p) { (*p)++; } // Orijinal adrese gidip artırır.
```

---

## 4. Öğrenme Köşesi: "Implicit Declaration" Uyarısı

Eğer bir fonksiyonu `main`'den sonra yazarsanız ve öncesinde tanıtmazsanız derleyici kızar.
```c
void selam(); // 1. Bildirim (Prototype / Prototip)

int main() {
    selam();
}

void selam() { // 2. Tanımlama (Definition)
    printf("Merhaba");
}
```
**Analoji:** Birini partiye çağırmadan önce, listenize adını eklemeniz (Prototip) gerekir.

---

## 5. StackOverflow Notları: "Function Pointer" Nedir?

Fonksiyonlar da bellekte bir adreste durur. Tıpkı değişkenler gibi, fonksiyonların da adresini bir pointer'da tutup, o pointer üzerinden fonksiyonu çağırabilirsiniz. Bu, gelişmiş C projelerinde (örneğin plugin sistemlerinde) çok yaygındır.

---

## 6. Gerçek Hayat: Stack Overflow Hatası

Eğer bir fonksiyonun içinde çok büyük bir dizi (örneğin `int dizi[1000000]`) tanımlarsanız veya sonsuz bir döngüyle fonksiyonun kendisini çağırmasına (recursion) neden olursanız, Stack'teki yeriniz biter. İşletim sistemi programınızı "Stack Overflow" hatasıyla sonlandırır.

---

*Fonksiyonun kendi kendini çağırması:* [02-ozyineleme-recursive.md](02-ozyineleme-recursive.md)
