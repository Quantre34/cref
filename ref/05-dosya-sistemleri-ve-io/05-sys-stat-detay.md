---
baslik: "sys/stat.h - Dosya Durumu ve Yetkiler"
kategori: "io"
etiketler: ["stat", "chmod", "mkdir", "permissions", "file-info"]
zorluk: "ileri"
ilgili: ["05-dosya-sistemleri-ve-io/04-unistd-ve-sistem-cagrilari.md"]
---

# sys/stat.h - Dosya Durumu ve Yetkiler

Bir dosyanın sadece içeriğiyle değil, "kimlik kartıyla" (metadata) ilgileniyorsanız `sys/stat.h` kullanmalısınız. Dosya boyutu, oluşturulma tarihi ve yetkiler bu kütüphane ile öğrenilir.

## 1. struct stat

Bir dosya hakkında her şeyi içeren dev bir yapıdır.

```c
struct stat info;
stat("dosya.txt", &info);

printf("Boyut: %lld byte\n", info.st_size);
printf("Sahibi: %d\n", info.st_uid);
printf("Son Erişim: %ld\n", info.st_atime);
```

---

## 2. Yetki Kontrolü ve Değiştirme

C ile dosya yetkilerini (Okuma/Yazma/Çalıştırma) değiştirebilirsiniz.

- `chmod(path, mode)`: Yetkileri değiştirir (Örn: `0755`).
- `mkdir(path, mode)`: Yeni bir klasör oluşturur.

---

## 3. Dosya Tipi Makroları

Bir öğenin klasör mü yoksa dosya mı olduğunu anlamak için şu makrolar kullanılır:
- `S_ISREG(m)`: Normal dosya mı?
- `S_ISDIR(m)`: Klasör mü?

---

## 4. Öğrenme Köşesi: "Octal" (Sekizlik) Sayılar

Yetkilerle çalışırken sayılar neden `0` ile başlar (Örn: `0644`)? C'de `0` ile başlayan sayılar sekizlik sistemdedir. Dosya yetkileri 3-bitlik gruplar (Sahibi, Grubu, Diğerleri) halinde tutulduğu için sekizlik sistem en doğal gösterimdir.

---

## 5. StackOverflow Notları: `stat` vs `fstat`

- `stat(path, ...)`: Dosyanın adını/yolunu kullanarak bilgi alır.
- `fstat(fd, ...)`: Zaten açılmış olan bir dosyanın (File Descriptor üzerinden) bilgisini alır. `fstat` daha güvenlidir çünkü dosya ismi değişse bile yanlış dosyaya bakmazsınız.

---

## 6. Gerçek Hayat: Yedekleme Yazılımları

Yedekleme programları (Örn: `rsync`), iki dosyanın içeriğini okumadan önce `stat` ile tarihlerine ve boyutlarına bakar. Eğer tarihler aynıysa dosyayı okumadan geçer. Bu, muazzam bir hız kazandırır.

---

*Dizinleri taramak:* [08-standart-kutuphane-referansi/dirent-detay.md](../08-standart-kutuphane-referansi/dirent-detay.md)
