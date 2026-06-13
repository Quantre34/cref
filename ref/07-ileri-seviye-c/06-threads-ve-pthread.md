---
baslik: "Threads (Çok Kanallı Programlama)"
kategori: "ileri"
etiketler: ["threads", "pthread", "mutex", "concurrency", "parallel"]
zorluk: "ileri"
ilgili: ["07-ileri-seviye-c/05-stdatomic-ve-atomik-islemler.md"]
---

# Threads (Çok Kanallı Programlama)

Modern bilgisayarlarda aynı anda birden fazla iş yapmak için **Thread** (İş parçacığı) kullanılır. C'de iki ana yol vardır: Standard C11 `<threads.h>` ve POSIX `<pthread.h>`.

## 1. Thread Nedir? (Mental Model)

Bir programı bir mutfak olarak düşünün.
- **Single-threaded:** Tek bir şef var. Önce çorbayı yapar, bitince salataya geçer.
- **Multi-threaded:** Birden fazla şef var. Biri çorbayı karıştırırken diğeri domatesleri doğrar.

---

## 2. POSIX Threads (pthread.h)

Linux ve macOS dünyasında en yaygın kullanılan kütüphanedir.

### Temel Fonksiyonlar:
1. `pthread_create()`: Yeni bir thread başlatır.
2. `pthread_join()`: Başka bir thread'in bitmesini bekler.
3. `pthread_exit()`: Mevcut thread'i sonlandırır.

```c
#include <pthread.h>
#include <stdio.h>

void* selam_ver(void* arg) {
    printf("Thread'den selamlar!\n");
    return NULL;
}

int main() {
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, selam_ver, NULL);
    pthread_join(thread_id, NULL); // Ana program beklesin
    return 0;
}
```

---

## 3. Mutex (Kilit Mekanizması)

Eğer iki thread aynı anda aynı değişkeni değiştirmeye çalışırsa kaos oluşur. Bunu önlemek için **Mutex** (Mutual Exclusion) kullanılır.

**Analoji:** Tuvalet anahtarı. İçeri giren anahtarı alır ve kapıyı kilitler. İşi bitince anahtarı bırakır, sıradaki kişi içeri girer.

```c
pthread_mutex_t kilit;
pthread_mutex_lock(&kilit);
// ... KRİTİK BÖLGE (Sadece tek thread girebilir) ...
pthread_mutex_unlock(&kilit);
```

---

## 4. Öğrenme Köşesi: "Deadlock" (Ölümcül Kilitlenme)

Thread A, 1. anahtarı almış 2.yi bekliyor. Thread B ise 2. anahtarı almış 1.yi bekliyor. İkisi de sonsuza kadar birbirini bekler. Program donar. Buna **Deadlock** denir.

---

## 5. StackOverflow Notları: "-lpthread" Bayrağı

POSIX threads kullanırken derleme komutuna mutlaka `-lpthread` eklemelisiniz:
```bash
gcc program.c -o program -lpthread
```

---

## 6. Gerçek Hayat: Oyun Motorları

Modern oyunlarda grafikler bir thread'de çizilirken, fizik hesaplamaları başka bir thread'de, sesler ise üçüncü bir thread'de işlenir. Bu sayede oyunlar akıcı çalışır.

---

*Donanım seviyesinde güvenlik:* [07-ileri-seviye-c/05-stdatomic-ve-atomik-islemler.md](05-stdatomic-ve-atomik-islemler.md)
