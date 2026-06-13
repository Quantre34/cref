---
baslik: "Vaka Analizi: Segmentation Fault Otopsisi"
kategori: "senaryo"
etiketler: ["segfault", "debugging", "backtrace", "gdb", "core-dump"]
zorluk: "ileri"
ilgili: ["09-ek-kaynaklar/gdb-ve-valgrind-rehberi.md"]
---

# Vaka Analizi: Segmentation Fault Otopsisi

Programın bir anda "Segmentation Fault" diyerek kapandı. Şimdi bir dedektif gibi bu cinayeti (çökmeyi) aydınlatalım.

## 1. Olay Yeri İncelemesi (GDB)
Cinayetin nerede işlendiğini bulmak için programı `gdb` ile çalıştırıyoruz.

```bash
gdb ./program
run
# (Çökme anında)
backtrace
```
`backtrace` komutu sana çökmenin tam olarak hangi fonksiyonun kaçıncı satırında olduğunu söyleyecektir.

## 2. Şüpheli: NULL Pointer
En sık rastlanan katil.
```c
char *p = get_username(id); // Fonksiyon kullanıcı bulunamazsa NULL dönüyor.
printf("%c", p[0]); // NULL'un 0. elemanına bakarsan SEGFAULT!
```

## 3. Şüpheli: Salt-Okunur Bölgeye Yazma
```c
char *s = "Merhaba"; // Bu metin 'Read-Only' bölgesindedir.
s[0] = 'H'; // BURADA ÇÖKER. Çünkü sabit bir metni değiştirmeye çalıştın.
```

## 4. Şüpheli: Stack Overflow (Sonsuz Özyineleme)
```c
int sonsuz(int n) { return sonsuz(n+1); } // Stack dolar ve çöker.
```

## 5. Çözüm Yolları (Otopsi Sonucu)
1. **Valgrind Koş:** `valgrind ./program`. Valgrind sana çökmeden hemen önce hangi yanlış hamleyi yaptığını söyleyen bir zaman makinesi gibidir.
2. **Kritik Kontroller:** Her pointer kullanımından önce `if(ptr)` kontrolü ekle.
3. **Address Sanitizer:** Derlerken `-fsanitize=address` bayrağını kullan. Bu bayrak sana renkli ve detaylı bir hata raporu sunar.

---

## Öğrenme Köşesi: "Core Dump" Dosyasını Okumak
Programın çöktüğünde oluşan `core` dosyası, programın öldüğü andaki RAM fotoğrafıdır. Bu dosyayı `gdb ./program core` ile açıp, sanki program hala çalışıyormuş gibi içindeki değişkenlere bakabilirsin.

---

*Gerçek dünya örnekleri:* [09-ek-kaynaklar/real-world-c-projects.md](../09-ek-kaynaklar/real-world-c-projects.md)
