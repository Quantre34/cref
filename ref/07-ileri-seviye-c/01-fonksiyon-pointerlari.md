---
baslik: "Fonksiyon Pointerları ve Callback"
kategori: "ileri"
etiketler: ["pointer", "fonksiyon-pointer", "callback", "qsort", "adres"]
zorluk: "ileri"
ilgili: ["01-fonksiyonlar-ve-stack-frame.md", "03-moduler-programlama/01-fonksiyonlar-ve-stack-frame.md"]
---

# Fonksiyon Pointerları ve Callback

Tıpkı bir `int` değişkeni gibi, fonksiyonlar da bellekte bir adreste durur. Bu adresi bir pointer'da saklayabilir ve fonksiyonu bu pointer üzerinden çağırabilirsiniz.

## 1. Sözdizimi (Syntax)

Fonksiyon pointer'ı tanımlamak biraz kafa karıştırıcı olabilir:
```c
// int dönen ve iki int alan bir fonksiyonu gösterebilir:
int (*islem_ptr)(int, int);
```

**Analoji: Telefon Rehberi**
Normalde fonksiyonu adıyla ararsınız. Fonksiyon pointer'ı ise o kişinin telefon numarasını (adresini) bir yere kaydetmek gibidir. Kimin numarasını kaydederseniz onu ararsınız.

---

## 2. Neden Kullanırız? (Callback Mekanizması)

Bir fonksiyonun içine "ne yapacağını" değil, "yapılacak işi yapan fonksiyonu" gönderebilirsiniz.

```c
void her_elemana_uygula(int *dizi, int boyut, void (*islem)(int)) {
    for (int i = 0; i < boyut; i++) {
        islem(dizi[i]);
    }
}
```

---

## 3. Gerçek Hayat Örneği: qsort()

C standart kütüphanesindeki `qsort` fonksiyonu, her şeyi (sayı, struct, string) sıralayabilir. Çünkü "karşılaştırma" işini size bırakır.

```c
int karsilastir(const void *a, const void *b) {
    return (*(int*)a - *(int*)b);
}

int main() {
    int dizi[] = {5, 2, 9, 1};
    qsort(dizi, 4, sizeof(int), karsilastir); // karsilastir bir callback'tir.
}
```

---

## 4. Öğrenme Köşesi: "Typedef" ile Kolaylaştırma

Karmaşık fonksiyon pointer'larını `typedef` ile basitleştirebilirsiniz:
```c
typedef int (*MatIslemi)(int, int);

MatIslemi f = topla;
```

---

## 5. StackOverflow Notları: Parantezlerin Önemi

- `int *f(int)` -> `int*` dönen bir **fonksiyon**.
- `int (*f)(int)` -> Bir fonksiyona işaret eden **pointer**.

---

## 6. Gerçek Hayat: Olay Yöneticileri (Event Handlers)

Oyunlarda bir butona tıklandığında ne olacağını belirlemek için fonksiyon pointer'ları kullanılır. Butonun içine "tıklanınca şu adresteki fonksiyonu çalıştır" dersiniz. Bu sayede buton koduyla, butona basınca olacak kod birbirine karışmaz.

---

*Değişken sayıda parametre almak:* [02-variadic-fonksiyonlar.md](02-variadic-fonksiyonlar.md)
