---
baslik: "Tanımsız Davranış (Undefined Behavior - UB) Rehberi"
kategori: "debug"
etiketler: ["ub", "undefined-behavior", "segfault", "security", "optimization"]
zorluk: "ileri"
ilgili: ["02-bellek-ve-veri-yonetimi/05-bellek-hatalari-ve-guvenlik.md"]
---

# Tanımsız Davranış (Undefined Behavior - UB) Rehberi

C'de bazı kodlar vardır ki, derleyici bunların ne yapacağını söylemek zorunda değildir. Bu durumlara **Tanımsız Davranış (UB)** denir. UB içeren bir program; bazen çalışır, bazen çöker, bazen de bilgisayarı ele geçiren bir virüse dönüşebilir.

## 1. Neden UB Var?
C'nin amacı "hız"dır. Derleyici, senin asla yanlış bir şey yapmayacağını varsayar. Eğer yanlış yaparsan (UB), derleyici bu hatayı kontrol etmekle vakit kaybetmez ve optimizasyon yaparken kodunu silebilir veya değiştirebilir.

---

## 2. En Ünlü UB Örnekleri

| Hata | Olası Sonuç |
|------|-------------|
| **Dizi Sınırını Aşmak** | Diğer değişkenlerin bozulması veya Segfault. |
| **Null Dereference** | Programın anında çökmesi. |
| **Signed Integer Overflow** | Sayının negatiften pozitife saçma sapan zıplaması. |
| **Başlatılmamış Değişkeni Okumak** | RAM'deki rastgele "çöp" verilerin okunması. |
| **free() Edilmiş Yeri Kullanmak** | Use-after-free (En tehlikeli güvenlik açığı). |
| **Aynı Değişkeni Aynı Satırda İki Kez Değiştirmek** | `i = i++ + ++i;` -> Sonuç belirsizdir. |

---

## 3. Derleyicinin UB'yi Kullanması (Nasal Demons)

Derleyiciler UB içeren kodu gördüğünde şunu diyebilir: "Yazılımcı burada hata yapmayacağına göre, bu `if` bloğu asla çalışmayacak." diyerek o bloğu silebilir. Bu, mantık hatalarına yol açar.

---

## 4. Öğrenme Köşesi: "Time Travel" Debugging

Bazen UB, hatanın olduğu satırdan çok daha **önce** veya **sonra** programın garip davranmasına neden olur. Buna "zaman yolculuğu" denir. Sorun bellek bozulması olduğu için hatayı bulmak çok zordur.

---

## 5. StackOverflow Notları: "UB vs Implementation-Defined"

- **Undefined Behavior:** Her şey olabilir (Çökme, silinme, çalışma).
- **Implementation-Defined:** Derleyiciden derleyiciye değişir ama dökümanda yazmak zorundadır (Örn: `int`'in kaç byte olduğu).

---

## 6. Gerçek Hayat: Exploitler

Hacklenen sistemlerin %90'ı, C kodundaki bir UB'yi (genellikle Buffer Overflow) kullanan saldırganlar tarafından ele geçirilir. UB'den kaçınmak sadece hatasız kod yazmak değil, aynı zamanda güvenli kod yazmaktır.

---

*Hataları ayıklama sanatı:* [gdb-ve-valgrind-rehberi.md](gdb-ve-valgrind-rehberi.md)
