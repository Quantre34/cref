---
baslik: "C Programının Anatomisi"
kategori: "temel"
etiketler: ["main", "program-yapisi", "return", "fonksiyon", "anatomisi"]
zorluk: "baslangic"
ilgili: ["00-kurulum-ve-hazirlik.md", "02-degiskenler-ve-bellek.md"]
---

# C Programının Anatomisi

Bir C dosyasına baktığınızda aslında bir "talimatlar hiyerarşisi" görürsünüz. Her satırın belirli bir amacı ve bellekte bir karşılığı vardır.

## 1. Kodun İskeleti

```c
#include <stdio.h>    // 1. Ön İşlemci Direktifi

int main(void) {      // 2. Giriş Noktası (Entry Point)
    printf("Selam!"); // 3. İfade (Statement)
    return 0;         // 4. Çıkış Kodu
}
```

### 1.1. #include <stdio.h> (Tedarik Zinciri)
Bu satır derleyiciye şunu söyler: "Bana standart giriş-çıkış kütüphanesini (`stdio`) getir. İçindeki `printf` fonksiyonunu kullanacağım." 
- **Under the Hood:** Derleyici bu satırı gördüğünde, `stdio.h` dosyasının içeriğini kopyalayıp tam bu satıra yapıştırır (Ön işleme aşaması).

### 1.2. int main(void) (Orkestra Şefi)
C programları her zaman `main` fonksiyonundan başlar.
- **int:** Fonksiyon bittiğinde işletim sistemine bir tam sayı (integer) vereceğini vaat eder.
- **void:** "Ben dışarıdan parametre almıyorum" demektir.
- **{} (Süslü Parantezler):** Fonksiyonun vücudunu (scope) sınırlar.

### 1.3. return 0; (Rapor Verme)
Program bittiğinde işletim sistemine durum raporu gönderir.
- `0`: "Her şey yolunda, görev başarıyla tamamlandı."
- `0 dışında her şey`: "Bir hata oluştu!"

---

## 2. Bellek Bakışı: Kod Nereye Gider?

Programın çalıştığında işletim sistemi ona bir miktar yer ayırır. Senin yazdığın bu kodlar **Code Segment** (veya Text Segment) denilen, salt-okunur bir bölgeye yüklenir. 

- `main` fonksiyonu çağrıldığında, işlemcinin "Instruction Pointer"ı (IP) senin kodunun başlangıç adresine zıplar.

---

## 3. Fonksiyon İmzası: printf

`printf` aslında karmaşık bir makinedir.
- **Girdi:** Bir format string (metin) ve isteğe bağlı değişkenler.
- **İşleyiş:** Metni analiz eder, özel karakterleri (`%d`, `%s`) bulur ve yerine değerleri koyar.
- **Çıktı:** Standart çıktıya (genelde ekran) karakterleri gönderir.

---

## 4. Öğrenme Köşesi: Noktalı Virgül (;) Neden Var?

C'de noktalı virgül bir **ayırıcı** değil, bir **sonlandırıcı**dır (terminator). 
- **Analoji:** Türkçedeki "nokta" (.) gibidir. Cümle bittiğinde nokta koymazsanız nerede duracağınızı bilemezsiniz. Derleyici de `;` görene kadar ifadeyi okumaya devam eder.

---

## 5. StackOverflow Notları: "void main()" Yanlışı

Bazı eski kitaplarda `void main()` görebilirsiniz. **ASLA KULLANMAYIN.**
C standartlarına göre `main` her zaman `int` dönmelidir. Bazı derleyiciler `void` kabul etse de, bu kodun taşınabilirliğini (portability) bozar ve "Undefined Behavior" riskine yol açar.

---

## 6. Gerçek Hayat: main'in Parametreleri

Aslında `main` sadece `void` almaz. Komut satırından gelen verileri de okuyabilir:
```c
int main(int argc, char *argv[]) {
    // argc: Kaç tane kelime girildi?
    // argv: Girilen kelimeler neler?
}
```
Örneğin `./program dosya.txt` yazdığınızda, programınız bu `dosya.txt` adını `argv` içinden okur.

---

*Değişkenlerin dünyasına yolculuk:* [02-degiskenler-ve-bellek.md](02-degiskenler-ve-bellek.md)
