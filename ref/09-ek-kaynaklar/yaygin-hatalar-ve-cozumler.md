---
baslik: "Yaygın Hatalar ve Çözümleri"
kategori: "debug"
etiketler: ["hata", "segfault", "debug", "cozum"]
zorluk: "tum-seviyeler"
ilgili: ["02-bellek-ve-veri-yonetimi/05-bellek-hatalari-ve-guvenlik.md"]
---

# Yaygın Hatalar ve Çözümleri

C dünyasında herkesin en az bir kez yaptığı "klasik" hatalar ve bunların hızlı çözümleri.

## 1. Segmentation Fault (Giriş Yasak!)
- **Neden:** Pointer `NULL` iken içindeki veriye bakmaya çalıştınız veya dizinin sınırını aştınız.
- **Çözüm:** Pointer'ı kullanmadan önce `if (ptr != NULL)` kontrolü yapın. Dizilerde `i < BOYUT` kuralına uyun.

## 2. Scanf ile Sonsuz Döngü
- **Neden:** `scanf("%d", &x);` yazdınız ama kullanıcı harf girdi. Harf tamponda (buffer) kalır ve `scanf` her seferinde onu okumaya çalışıp başarısız olur.
- **Çözüm:** `scanf`'in dönüş değerini kontrol edin veya her okumadan sonra `while(getchar() != '\n');` ile tamponu temizleyin.

## 3. Neden Değişkenim Değişmiyor?
- **Neden:** Fonksiyona değişkenin kendisini (`x`) gönderdiniz. Fonksiyon onun kopyasıyla çalıştı.
- **Çözüm:** Fonksiyona adresi (`&x`) gönderin ve fonksiyonun içinde pointer (`*x`) kullanın.

## 4. Kaybolan Veriler (Local Variables)
- **Neden:** Bir fonksiyonun içindeki yerel bir dizinin adresini `return` ettiniz. Fonksiyon bitince o dizi yok oldu!
- **Çözüm:** Ya veriyi `malloc` ile Heap'te oluşturun ya da diziyi fonksiyona dışarıdan (parametre olarak) gönderin.

---

## Öğrenme Köşesi: "Rubber Duck Debugging"
Eğer bir hatayı bulamıyorsanız, masanızdaki bir **plastik ördeğe** (veya cansız bir nesneye) kodunuzu satır satır anlatın. Sorunu anlatırken beyniniz mantık hatasını kendiliğinden fark edecektir. Bu bilimsel olarak kanıtlanmış bir tekniktir.

---

*Hata ayıklama araçları:* [gdb-ve-valgrind-rehberi.md](gdb-ve-valgrind-rehberi.md)
