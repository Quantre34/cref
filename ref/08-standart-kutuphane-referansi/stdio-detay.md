---
baslik: "stdio.h - Standart Giriş/Çıkış"
kategori: "kutuphane"
etiketler: ["stdio", "printf", "scanf", "file", "io"]
zorluk: "tum-seviyeler"
ilgili: ["05-dosya-sistemleri-ve-io/01-standart-io-akislari.md"]
---

# stdio.h - Standart Giriş/Çıkış

C'nin en temel kütüphanesidir. Tip kısıtlamalarına uymak için fonksiyon imzalarını (input/output tiplerini) doğru bilmek gerekir.

## 1. Yazdırma Fonksiyonları

| Fonksiyon İmzası | Dönüş Tipi | Açıklama |
|------------------|------------|----------|
| `int printf(const char *format, ...)` | `int` | `stdout`'a yazar. Yazılan karakter sayısını döner. |
| `int fprintf(FILE *stream, const char *format, ...)` | `int` | Belirli bir akışa (dosya, stderr vb.) yazar. |
| `int sprintf(char *str, const char *format, ...)` | `int` | Metni bir diziye (buffer) yazar. |
| `int snprintf(char *str, size_t size, ...)` | `int` | **Güvenli:** Belirli bir boyutu aşmadan diziye yazar. |

---

## 2. Okuma Fonksiyonları

| Fonksiyon İmzası | Girdi Tipleri | Açıklama |
|------------------|---------------|----------|
| `int scanf(const char *format, ...)` | `const char*, pointerlar` | `stdin`'den okur. Başarılı okunan madde sayısını döner. |
| `int fscanf(FILE *stream, ...)` | `FILE*, ...` | Dosyadan biçimlendirilmiş veri okur. |
| `int sscanf(const char *str, ...)` | `char*, ...` | Metin dizisinden veri okur. |
| `char *fgets(char *str, int n, FILE *stream)` | `char*, int, FILE*` | **Güvenli:** Satır okur. Başarılıysa `str`'yi, hata/EOF ise `NULL` döner. |
| `int getchar(void)` | `void` | Tek bir karakter okur ve `int` döner. |

---

## 3. Dosya Yönetimi

| Fonksiyon İmzası | Dönüş Tipi | Açıklama |
|------------------|------------|----------|
| `FILE *fopen(const char *path, const char *mode)` | `FILE *` | Dosya açar. Hata durumunda `NULL` döner. |
| `int fclose(FILE *stream)` | `int` | Dosyayı kapatır. Başarıda 0 döner. |
| `size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)` | `size_t` | İkili (binary) okuma yapar. |
| `size_t fwrite(const void *ptr, size_t size, size_t n, FILE *f)` | `size_t` | İkili (binary) yazma yapar. |

---

## 4. Öğrenme Köşesi: `perror()` Fonksiyonu

`void perror(const char *s)`: Son hatayı (errno) okur ve kullanıcı mesajıyla birleştirip `stderr`'e basar.

---

## 5. StackOverflow Notları: `int` dönen `getchar()`

`getchar()` neden `char` değil de `int` döner? Çünkü `EOF` (End Of File) değeri genellikle `-1`'dir ve standart bir `char` bu değeri tutamayabilir. Tipleri karıştırmamak için dönüşü her zaman `int` bir değişkende saklayın.

---

*Bellek ve yardımcı araçlar:* [stdlib-detay.md](stdlib-detay.md)
