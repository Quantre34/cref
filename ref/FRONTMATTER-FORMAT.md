---
baslik: "Frontmatter Formatı (Arama Motoru için)"
kategori: "meta"
etiketler: ["meta", "format", "arama-motoru", "frontmatter"]
zorluk: "baslangic"
---

# Frontmatter Formatı

Her `.md` dosyasının başında `---` ile sınırlandırılmış YAML bloğu bulunur.
C ile yazacağın arama motorunun bu formatı ayrıştırması için referans.

## Format Şeması

```
---
baslik: "Dosyanın tam başlığı"
kategori: "tek-kelime-slug"
etiketler: ["etiket1", "etiket2", "etiket3"]
zorluk: "baslangic" | "orta" | "ileri" | "tum-seviyeler"
ilgili: ["diger-dosya.md", "baska-dosya.md"]
---
```

## Alanlar

| Alan | Tip | Açıklama |
|------|-----|----------|
| `baslik` | string | Dosyanın okunabilir başlığı (tırnak içinde) |
| `kategori` | string | Ana kategori slug'ı (tırnak yok) |
| `etiketler` | string dizisi | Arama etiketleri (köşeli parantez içinde, tırnaklı) |
| `zorluk` | string | Zorluk seviyesi |
| `ilgili` | string dizisi | İlgili dosyaların isimleri (opsiyonel) |

## Kategoriler

| kategori | Anlamı |
|----------|--------|
| `index` | Dizin dosyaları |
| `temel` | Başlangıç konuları |
| `bellek` | Bellek ve pointer |
| `veri-yapilari` | Struct, dizi, koleksiyonlar |
| `io` | Dosya ve giriş/çıkış |
| `kutuphane` | Standart kütüphane |
| `derleme` | Derleme ve ön işlemci |
| `debug` | Hata ayıklama |
| `ileri` | İleri seviye konular |
| `meta` | Rehber hakkında meta bilgi |

## C ile Ayrıştırma Rehberi

Arama motorunu yazarken şu adımları izle:

### 1. Frontmatter Tespiti
```
Dosyayı oku → İlk satır "---" ise frontmatter başlıyor
"---" ile biten blok okunana kadar satır satır oku
Son "---" bulunca frontmatter bitiyor, içerik başlıyor
```

### 2. Alan Ayrıştırma
```
Her satır için:
  ":" karakterinden önce: alan adı (trim yap)
  ":" karakterinden sonra: değer (trim yap)

Etiket dizisi örneği:
  etiketler: ["tag1", "tag2", "tag3"]
  
  '[' karakterini bul
  ']' karakterine kadar oku
  '"' ile '"' arasındaki stringleri çıkar
  ',' ile ayır
```

### 3. Tam Metin Arama
```
Frontmatter sonrasındaki tüm içerik (Markdown metni)
strstr() veya Boyer-Moore ile anahtar kelime arama
Büyük/küçük harf duyarsız için önce tolower() uygula
```

## Örnek C Arama Motoru Yapısı

```c
typedef struct {
    char  dosya_yolu[256];
    char  baslik[256];
    char  kategori[64];
    char  etiketler[20][64];  // En fazla 20 etiket
    int   etiket_sayisi;
    char  zorluk[32];
    // Tam metin için dosyayı tekrar aç
} DosyaMeta;

// Arama tipleri:
typedef enum {
    ARA_TAM_METIN,   // İçerikte geçiyor mu?
    ARA_BASLIK,      // Başlıkta geçiyor mu?
    ARA_ETIKET,      // Etiket eşleşmesi
    ARA_KATEGORI,    // Kategori eşleşmesi
    ARA_ZORLUK,      // Zorluk filtresi
} AramaTipi;
```

## Kullanım Önerileri

### Arama Örnekleri (Arama Motorunu Yazınca)
```bash
./c-ara "pointer"           # Tam metin + etiket araması
./c-ara -e "bellek"         # Sadece etiket araması
./c-ara -k "debug"          # Kategori araması
./c-ara -z orta             # Orta zorluk filtresi
./c-ara -b "Derleme"        # Başlık araması
./c-ara "malloc" -z ileri   # Kombinasyon
```

### Hızlı grep Araması (C motoru hazır olmadan)
```bash
# Frontmatter'da etiket ara
grep -l "pointer" /path/to/c-rehberi/*.md

# Tüm dosyalarda içerik ara
grep -rn "malloc" /path/to/c-rehberi/

# Büyük/küçük harf duyarsız
grep -ri "segfault" /path/to/c-rehberi/

# Satır numarasıyla
grep -n "buffer overflow" /path/to/c-rehberi/17-yaygin-hatalar.md

# Hangi dosyalarda etiket var?
grep -l '"pointer"' /path/to/c-rehberi/*.md
```
