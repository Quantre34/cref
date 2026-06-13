---
baslik: "Gerçek Dünya C Projeleri"
kategori: "ileri"
etiketler: ["projects", "opensource", "linux", "redis", "git"]
zorluk: "tum-seviyeler"
ilgili: ["01-program-yapisi.md"]
---

# Gerçek Dünya C Projeleri

C öğrendikten sonra "Peki bununla ne yapabilirim?" diye soruyorsanız, dünyayı yöneten yazılımların çoğunun C ile yazıldığını bilmelisiniz. İşte inceleyebileceğiniz bazı dev projeler:

## 1. Linux Çekirdeği (Kernel)
Dünyanın en büyük C projesi. Telefonunuzdan (Android), sunuculara ve süper bilgisayarlara kadar her şey bunun üzerinde çalışır.
- **Neden İncelemeli?** İleri seviye pointer kullanımı, donanım sürücüleri ve bellek yönetimi için en iyi kaynak.

## 2. Redis
Dünyanın en hızlı veritabanlarından biri. Tamamen C ile yazılmıştır.
- **Neden İncelemeli?** Çok temiz, okunabilir bir kodu vardır. Veri yapılarını (bağlı listeler, hash tabloları) gerçek hayatta nasıl kullanacağınızı görmek için harikadır.

## 3. Git
Versiyon kontrol sistemi olan Git'in çekirdeği C ile yazılmıştır.
- **Neden İncelemeli?** Dosya sistemleri ve veri sıkıştırma konularında müthiş örnekler içerir.

## 4. Doom (1993)
Efsanevi oyun Doom, C ile yazılmıştır.
- **Neden İncelemeli?** Kısıtlı donanımlarda nasıl yüksek performanslı 3D grafik (Raycasting) yapılacağını öğrenmek için bir klasik.

---

## Küçük Bir Proje Önerisi: Kendi "Shell"ini Yaz!
Öğrendiklerini pekiştirmek için terminalde çalışan basit bir komut satırı arayüzü (Shell) yazabilirsin:
1. `fgets` ile komutu oku.
2. `strtok` ile komutu ve argümanları parçala.
3. `fork` ve `exec` (Linux) kullanarak diğer programları çalıştır.

---

*Öğrendiklerini test etmek için ana dizine dön:* [INDEX.md](../INDEX.md)
