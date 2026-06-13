---
baslik: "Kurulum ve Geliştirme Ortamı"
kategori: "temel"
etiketler: ["gcc", "clang", "derleme", "makefile", "setup", "gdb"]
zorluk: "baslangic"
ilgili: ["01-program-yapisi.md", "06-onislemci-ve-derleme-sirlari/03-derleme-asamalari-derinlemesine.md"]
---

# Kurulum ve Geliştirme Ortamı

C dünyasına hoş geldin. C, "yakın metal" (close to the metal) bir dildir; yani bilgisayarın donanımıyla doğrudan konuşur. Bu yüzden kodun çalışmadan önce bir "tercüman"dan (derleyici) geçmelidir.

## 1. Neden Derleyici?

Bilgisayarlar C kodunu (`int main()`) anlamaz. Onlar sadece `0` ve `1` (makine kodu) anlar. Derleyici (Compiler), senin yazdığın insan tarafından okunabilir metni, işlemcinin anlayacağı komutlara dönüştürür.

### Mental Model: Mutfak Analojisi
- **Kaynak Kod (.c):** Yemek tarifi (İnsan okur).
- **Derleyici (GCC/Clang):** Şef (Tarifi alır, malzemeleri hazırlar).
- **Çalıştırılabilir Dosya (Binary):** Hazır yemek (Bilgisayar "yer" yani çalıştırır).

---

## 2. Araç Çantası (Toolchain) Kurulumu

### macOS: Apple'ın Yolu
macOS üzerinde en iyi deneyim için `Clang` kullanılır.
```bash
xcode-select --install
```

### Linux (Ubuntu/Debian): Standart Yol
Geliştirme paketlerinin tamamını tek seferde kurun:
```bash
sudo apt update
sudo apt install build-essential gdb valgrind
```
*`build-essential` içinde `gcc`, `make` ve temel kütüphaneler bulunur.*

### Windows: WSL2 (Önerilen)
Windows üzerinde doğrudan C geliştirmek zordur. **WSL2** (Windows Subsystem for Linux) kurarak gerçek bir Linux ortamında çalışmak en profesyonel yaklaşımdır.

---

## 3. İlk Derleme: "Hello World"ün Arkası

```c
#include <stdio.h>

int main(void) {
    printf("Merhaba C!\n");
    return 0;
}
```

Bu dosyayı `hello.c` adıyla kaydettiğini varsayalım. Sadece şu komutu koş:
```bash
gcc hello.c -o hello
```

### Burada Ne Oldu? (İşleyiş)
1. **Girdi:** `hello.c` (metin dosyası).
2. **İşlem:** GCC dosyayı okudu, sözdizimini kontrol etti, kütüphaneleri bağladı.
3. **Çıktı:** `hello` (makine kodu). Bu dosyayı `cat hello` ile okumaya çalışırsan saçma sapan karakterler görürsün; çünkü o artık işlemci içindir.

---

## 4. Kritik Derleyici Bayrakları (Flags)

Profesyonel C geliştiricileri asla sadece `gcc dosya.c` yazmaz. Derleyiciyi "titiz" olmaya zorlarlar.

| Bayrak | Anlamı | Neden Kullanmalısın? |
|--------|--------|----------------------|
| `-Wall` | **W**arnings **All** | Yazım hataları dışındaki mantık risklerini bildirir. |
| `-Wextra` | Extra Warnings | Daha da titiz kontrol sağlar. |
| `-std=c11` | Standard C11 | Modern C kurallarını takip etmeni sağlar. |
| `-g` | Debug Info | Hata ayıklarken (GDB) satır satır görmeni sağlar. |
| `-o` | Output Name | Çıktı dosyasına isim verir (yoksa varsayılan `a.out` olur). |

**İdeal Komut:**
```bash
gcc -Wall -Wextra -std=c11 -g hello.c -o hello
```

---

## 5. Öğrenme Köşesi: "Implicit Declaration" Hatası

Yeni başlayanların en çok karşılaştığı hata: `#include <stdio.h>` satırını unutmak.

**Deney:**
`#include` satırını sil ve derlemeye çalış. Derleyici sana şunu diyecektir:
`warning: implicit declaration of function 'printf'`

**Açıklama:** Derleyici `printf`'in ne olduğunu bilmiyor. "Sanırım böyle bir şey var ama emin değilim" diyor. C'de her şeyi kullanmadan önce derleyiciye "tanıtmalısın".

---

## 6. StackOverflow Notları: "a.out" Nedir?

Eğer `-o` bayrağını kullanmazsan, çıktı dosyası her zaman `a.out` (assembler output) olur. Bu bir C geleneğidir. 1970'lerden kalma bir alışkanlıktır.

---

## 7. Gerçek Hayat: Derleme Neden Uzun Sürer?

Büyük projelerde (Linux çekirdeği gibi) milyonlarca satır kod vardır. Tek bir derleme saatler sürebilir. Bu yüzden **Makefile** kullanarak sadece "değişen" dosyaları derleriz. Buna **Incremental Build** denir.

---

*Bir sonraki adımda programın anatomisini inceleyeceğiz:* [01-program-yapisi.md](01-program-yapisi.md)
