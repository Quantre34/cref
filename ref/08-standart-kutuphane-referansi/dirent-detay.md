---
baslik: "dirent.h - Dizin İşlemleri"
kategori: "kutuphane"
etiketler: ["dirent", "opendir", "readdir", "directory", "filesystem"]
zorluk: "orta"
ilgili: ["05-dosya-sistemleri-ve-io/02-dosya-okuma-yazma-teknikleri.md"]
---

# dirent.h - Dizin İşlemleri

`dirent.h`, bilgisayarınızdaki klasörleri (dizinleri) taramanıza ve içindeki dosyaları listelemenize olanak tanır. POSIX uyumlu bir kütüphanedir (Linux, macOS).

## 1. Temel Yapılar

### `DIR`
Diziyi temsil eden bir akış (stream) yapısıdır. `FILE` yapısına benzer.

### `struct dirent`
Dizinin içindeki her bir öğeyi (dosya veya klasör) temsil eden yapıdır.
```c
struct dirent {
    ino_t          d_ino;       // Inode numarası
    char           d_name[256]; // Dosyanın adı (EN ÖNEMLİ ALAN)
    unsigned char  d_type;      // Öğenin tipi (Dosya mı, Klasör mü?)
};
```

---

## 2. Temel Fonksiyonlar

### `DIR *opendir(const char *name)`
- **Görevi:** Bir dizini açar. Başarısız olursa `NULL` döner.

### `struct dirent *readdir(DIR *dirp)`
- **Görevi:** Dizinden bir sonraki öğeyi okur. Okunacak öğe kalmadıysa `NULL` döner.

### `int closedir(DIR *dirp)`
- **Görevi:** Dizin akışını kapatır.

---

## 3. Örnek: Bir Dizindeki Dosyaları Listeleme

```c
#include <stdio.h>
#include <dirent.h>

int main() {
    DIR *d = opendir("."); // Mevcut dizini aç
    struct dirent *dir;
    
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            printf("%s\n", dir->d_name);
        }
        closedir(d);
    }
    return 0;
}
```

---

## 4. Öğrenme Köşesi: `.` ve `..` Nedir?

Bir dizini taradığınızda her zaman `.` (mevcut dizin) ve `..` (üst dizin) isimlerini görürsünüz. Eğer sadece gerçek dosyaları istiyorsanız, bu isimleri bir `if` kontrolüyle atlamalısınız.

---

## 5. StackOverflow Notları: `d_type` Güvenilirliği

`dir->d_type` alanı (DT_REG: normal dosya, DT_DIR: klasör) her dosya sisteminde çalışmayabilir. En garanti yol, `stat()` fonksiyonunu kullanarak öğenin tipini doğrulamaktır.

---

## 6. Gerçek Hayat: "c-ara" Uygulaması

Şu an kullandığın `c-ara` arama motoru, `c-rehberi` klasöründeki dosyaları bulmak için arka planda tam olarak bu `dirent.h` kütüphanesini kullanır. Klasörleri tek tek gezer ve `.md` uzantılı olanları ayıklar.

---

*Dosya detaylarına erişmek:* [sys-stat-detay.md](sys-stat-detay.md)
