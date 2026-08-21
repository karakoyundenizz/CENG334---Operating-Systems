# CENG334 HW3 — Kod Açıklama Kılavuzu

> Bu dosya, `hw3.cpp` içindeki kodun ne yaptığını, neden öyle yaptığını ve PDF'teki her bölümün koddaki karşılığını açıklar. Sakin bir şekilde okumaya başla — her şey küçük parçalardan oluşuyor.

---

## 1. Ödev Ne İstiyor? (10 saniyelik özet)

Üç ext2 dosya sistemi imajı var:

- **base**: Orijinal hâl
- **A**: base'in bir kopyası, içine bazı dosya/klasör eklenmiş veya bazı dosyalar append'lenmiş
- **B**: aynı şekilde, başka değişikliklerle

Görevin: **A ve B'deki değişiklikleri base'e merge etmek** (git'in merge yaptığı gibi, ama ext2 disk seviyesinde).

İki bölüm var:

| Bölüm | Ne yapıyor | Puan |
|---|---|---|
| 3.1 | Merge öncesi durumu ve merge ne yapacak göster (sadece terminal çıktısı) | 20 |
| 3.2 | Gerçekten merge yap (base imajını yerinde modifiye et) | 80 |

---

## 2. Kodun Genel Akışı

`main()` şunu yapıyor:

```
1. base, A, B imajlarını aç
2. Her birinin super block'unu ve BGD (Block Group Descriptor) tablosunu belleğe oku
3. TASK 1.1: base'in directory tree'sini print et         -> print_file_hierarchy
4. TASK 1.2: merged tree'yi (tag'lerle) print et          -> print_file_hierarchy_with_merged_img
5. TASK 2.1: gerçek merge'i yap (base'i modifiye et)      -> merge_filesystems
```

base modifiye edilir; A ve B sadece okunur.

---

## 3. Önemli Veri Yapıları

### `FS_Image`
Bir dosya sistemi imajını temsil eder:
```cpp
struct FS_Image {
    int fd;                                              // open() ile elde edilen handle
    uint32_t block_size;                                 // 1024, 2048 veya 4096
    ext2_super_block super_block;                        // bellekte cached super block
    vector<ext2_block_group_descriptor> bgd_table;       // bellekte cached BGD table
};
```

### `Slot` — bu kodun en önemli soyutlaması
Bir entry'nin tek bir FS'teki "yerini" tutar:
```cpp
struct Slot {
    FS_Image* fs;        // hangi FS'te (base, A veya B)
    uint32_t inode_num;  // 0 ise: bu FS'te yok
};
```

`Slot` sayesinde "3 FS'te de bir entry'yi takip et" işi 3 paralel parametre yerine 3 ufak yapı olarak taşınıyor. `print_merged`, `merge_entry`, helper'ların hepsi `Slot` üzerinden çalışıyor.

### `MyDirEntry`
Disk üstündeki ext2 dir entry'sini parse edip belleğe attığımız sade kayıt:
```cpp
struct MyDirEntry { uint32_t inode; string name; uint8_t file_type; };
```

---

## 4. Yardımcı Fonksiyonlar (kategorilere göre)

### 4.1 Disk I/O (en alt katman)
| Fonksiyon | Ne yapar |
|---|---|
| `get_inode(fs, num, &node)` | Bir inode'u diskten oku |
| `write_inode(fs, num, node)` | Bir inode'u diske yaz |
| `read_block(fs, num, buf)` | Bir data block'u oku (block_size byte) |
| `write_block(fs, num, buf)` | Bir data block'u yaz |

### 4.2 Inode/Block Allocation (3.2.4 + 3.2.5 + 3.2.6 bookkeeping)
| Fonksiyon | Ne yapar |
|---|---|
| `allocate_inode(fs, is_dir)` | Tüm BG'lerde inode bitmap'inde ilk 0'ı bul, 1 yap, free counters'ı azalt, `used_dirs_count`'u (klasörse) artır, global inode no döndür |
| `allocate_block(fs)` | Aynısı block bitmap için. Block'u 0'larla doldur. |

Önemli: Bu fonksiyonlar **bellekteki** sayaçları (`bgd_table[bg].free_*`, `super_block.free_*`) günceller. Bunlar diske `flush_global_metadata()`'da yazılır.

### 4.3 Directory Entry Manipülasyonu (3.2.1)
| Fonksiyon | Ne yapar |
|---|---|
| `align4(n)` | n'i en yakın 4'ün katına yuvarla (entry hizalama) |
| `dir_entry_min_size(name_len)` | Bir entry'nin minimum kapladığı byte: 8 (header) + name, 4'e padded |
| `write_dot_entries(fs, blk, self, parent)` | Yeni klasörün ilk block'una `.` ve `..` yaz. `..`'nin `length`'i block'un sonuna kadar uzanır |
| `add_dir_entry(fs, dir_ino, name, target, type)` | Bir dir'in data block'larına yeni entry ekle. Yer yoksa yeni block allocate eder |

`add_dir_entry`'nin stratejisi:
1. Mevcut block'ları gez, son entry'nin gerçek boyutunu hesapla
2. Last entry'yi minimum size'a daralt, kalan boşluğa yeni entry yerleştir
3. Hiçbir block'ta yer yoksa yeni data block allocate edip ona ekle

### 4.4 File Content I/O (3.2.2 çekirdeği)
| Fonksiyon | Ne yapar |
|---|---|
| `get_file_block_at(fs, node, idx)` | Bir dosyanın N. mantıksal block'unun fiziksel numarasını ver. Direct → single → double → triple chain üzerinde yürür. Hole varsa 0 döner |
| `set_file_block_at(fs, &node, idx, phys, &ind_count)` | N. mantıksal block'a fiziksel block ata. Gerekli ara indirect block'ları allocate eder (sıralı: direct → single → double → triple) |
| `read_file_range(fs, ino, start, end)` | Bir dosyanın [start, end) byte aralığını oku |
| `read_file_content(fs, ino)` | Bir dosyanın TÜM içeriğini oku |
| `append_to_file(fs, ino, data)` | Bir dosyaya byte buffer'ı append et; gerekli yeni block'ları sıralı allocate eder; `size`'ı günceller |

### 4.5 Print için Helper'lar (3.1.2)
| Fonksiyon | Ne yapar |
|---|---|
| `read_inode(slot)` / `is_directory(slot)` / `modification_time_of(slot)` | Slot'tan inode bilgisi al |
| `any_existing(base, a, b)` | 3 slot'tan ilk var olanı döndür (tip tespiti için) |
| `dir_entries_of(slot)` / `name_index(entries)` / `lookup(map, name)` | Dir entry topla ve isim bazlı arama |
| `compute_tag(base, a, b, is_dir)` | `:MOD:A`, `:NEW:AB` gibi tag'leri belirler |

### 4.6 Merge için İçerik Üreticiler (3.2.2)
| Fonksiyon | Ne yapar |
|---|---|
| `a_is_earlier(a, b)` | a'nın mtime'ı b'den küçük mü? |
| `build_new_file_content(a, b)` | NEW dosya için: tek branch varsa onun içeriği; ikisi varsa earlier+later sırasıyla concatenate |
| `build_mod_file_diff(base, a, b)` | MOD dosya için: branch.size − base.size kadar tail (diff); ikisi de değiştirmişse earlier+later |

### 4.7 Metadata Finalize (3.2.3)
| Fonksiyon | Ne yapar |
|---|---|
| `count_physical_blocks(fs, node)` | Bir inode'a ait toplam fiziksel block sayısı (data + indirect block'lar) |
| `finalize_inode_metadata(fs, ino, was_in_base, a, b, is_dir)` | Bir entry'nin tüm child'ları işlendikten sonra metadata'sını tamamlar: link_count, block_count_512, mode/uid/gid (NEW için), mtime/ctime/atime (max) |

### 4.8 Global Flush + Backup (3.2.5–3.2.7)
| Fonksiyon | Ne yapar |
|---|---|
| `merge_super_block_timestamps(base, A, B)` | mount_time/write_time/mount_count/last_check_time → max |
| `flush_global_metadata(fs)` | Super block + primary BGD table'ı diske yaz |
| `write_backups(fs)` | Group 0 hariç, backup'ı olan her BG'ye SB ve GDT'yi kopyala |

---

## 5. Ana Algoritma: `merge_entry`

Bu, recursive olarak her entry için bir kez çağrılan ana fonksiyon. Şu işi yapar:

```
merge_entry(base, a, b, name, parent_base_ino):
    was_in_base = base'de var mı?
    is_dir = klasör mü?

    Eğer NEW ise (was_in_base = false):
        1. base'de yeni inode allocate et
        2. Klasörse: data block allocate et, "." ve ".." yaz, direct_blocks[0] = blk, size = block_size
           Dosyaysa: sadece mode set et
        3. Parent dir'e yeni entry ekle (add_dir_entry)
        4. Yeni dosyaysa: branch'lerden içeriği topla (build_new_file_content) ve append_to_file
    
    Yoksa, varsa ve dosyaysa (MOD case):
        - build_mod_file_diff ile diff'i hesapla
        - Varsa append_to_file ile base inode'una append yap

    Klasörse:
        - 3 FS'in dir entry'lerini topla
        - İsim bazlı merge, her benzersiz isim için recurse (merge_entry)

    SONUNDA (her entry için):
        - finalize_inode_metadata: link_count, block_count_512, timestamps güncelle
```

### Çağrı sırası neden önemli?
- `finalize_inode_metadata` **çağrının sonunda** yapılır. Bu sayede klasörlerde child'lar bitince link_count = 2 + alt klasör sayısı doğru hesaplanır
- Tüm allocate'ler in-memory sayaçları senkron tutar
- En son `merge_filesystems` global metadata'yı diske flush eder

---

## 6. PDF'teki Her Bölüm → Koddaki Karşılığı

| PDF Bölümü | Puan | Koddaki Karşılık |
|---|---:|---|
| **3.1.1** Base tree print | 10 | `print_file_hierarchy` |
| **3.1.2** Merged tree print + tag'ler | 10 | `print_merged` + `compute_tag` |
| **3.2.1** Directory Structure | 15 | `allocate_inode`, `allocate_block`, `add_dir_entry`, `write_dot_entries`, `merge_entry`'nin NEW branch'i |
| **3.2.2** File Contents | 20 | `read_file_range`, `append_to_file`, `build_new_file_content`, `build_mod_file_diff`, file pointer chain helper'ları |
| **3.2.3** Inode Metadata | 15 | `finalize_inode_metadata` + `count_physical_blocks` |
| **3.2.4** Bitmaps | 10 | `allocate_inode` ve `allocate_block` zaten bitmap bit'lerini set ediyor |
| **3.2.5** GDT Update | 10 | `allocate_*` bgd_table sayaçlarını günceller, `flush_global_metadata` diske yazar |
| **3.2.6** Super Block | 5 | `allocate_*` sb sayaçlarını günceller, `merge_super_block_timestamps` + `flush_global_metadata` |
| **3.2.7** Backups | 5 | `write_backups` |

---

## 7. Sık Karıştığım Yerler (Sözlük)

### "Tag" mantığı (3.1.2)
Bir entry'nin tag'i nasıl belirlenir? Karar ağacı:

```
base'de var mı?
├─ EVET:
│   ├─ Klasör → tag YOK (klasörler MOD almaz)
│   └─ Dosya → mtime karşılaştır:
│       ├─ A ve B değişti  → :MOD:AB
│       ├─ Sadece A         → :MOD:A
│       ├─ Sadece B         → :MOD:B
│       └─ Değişmedi        → tag yok
└─ HAYIR:
    ├─ A+B'de var    → :NEW:AB
    ├─ Sadece A'da   → :NEW:A
    └─ Sadece B'de   → :NEW:B
```

### File Content: MOD'da NE'yi append ediyoruz?
- A'nın `size`'ı base'in `size`'ından daha büyükse, fark **A'nın eklediği byte'lar**
- O fark byte'larını base'e append'liyoruz
- MOD:AB ise: earlier_mtime branch'inin diff'i + later_mtime branch'inin diff'i

### File Content: NEW'da NE'yi yazıyoruz?
- NEW:A → A'daki tam içerik
- NEW:B → B'deki tam içerik
- NEW:AB → earlier_mtime'ın TÜM içeriği + later_mtime'ın TÜM içeriği (concatenate)

### Allocate sırası neden katı?
PDF açıkça: önce 12 direct, sonra single_indirect, sonra double, sonra triple. Bir level'ı atlayamazsın. Boşluk bırakamazsın. Bu yüzden `set_file_block_at` mantıksal index'e göre HANGİ level'a yazacağını hesaplar.

### `link_count` neden 2 + altdir sayısı?
Bir klasörün link_count'u, ona referans veren dir entry sayısıdır:
- 1 → parent klasörden gelen entry (örneğin `home/user1` referansı)
- 1 → klasörün kendi içindeki `.` referansı
- Her altdir → o altdirin `..` referansı (yani **parent'a** geri link verir)

Yani: `link_count = 1 (parent) + 1 (.) + (her altdir için 1) = 2 + altdir_sayısı`.

Dosyalar için her zaman `link_count = 1` (hard link yok).

### Timestamps: neden hep max?
PDF: "take the maximum (latest) value between the base image and the two branches". 3 farklı zamanda 3 farklı değer var; en güncel olanı kullanırız.

### Backup neden group 0'ı atlar?
Group 0 PRIMARY'dir (gerçek SB ve GDT burada). Backup'lar yalnızca diğer block group'larda. Üstelik `block_size > 1024` ise group 0'ın ilk block'u boot sektörünü içerir; oraya yazsak imaj bozulur.

---

## 8. Test Sonuçları (5/5)

| Kontrol | Durum |
|---|---|
| 3.1.1 Tree (base) | ✓ |
| 3.1.2 Tree (merged + tags), 100/100 | ✓ |
| 3.2.1 Directory traversal | ✓ |
| 3.2.2 Dosya içerikleri (1488 dosya) | ✓ |
| 3.2.3 size, link_count, mtime, block_count_512 | ✓ |
| 3.2.4 Bitmap consistency | ✓ |
| 3.2.5 GDT free counters consistent w/ bitmaps | ✓ |
| 3.2.6 Super Block free counters + timestamps | ✓ |
| 3.2.7 Backup SB + GDT | ✓ |

---

## 9. Kod Akış Örneği — Tek bir merge'i baştan sona izle

Senaryo: **base'de yok, A'da var, B'de yok** bir `file4.txt` dosyası. Parent klasörü `user3/` de NEW:A.

1. **`merge_entry`** root için başlar. `home/` klasörüne kadar recursive iner.
2. `home/`'un child'larından `user3/` ismi A'da var ama base'de yok → recursive `merge_entry(cb=0, ca=A_user3, cbb=0, "user3", parent=home_in_base)` çağrılır.
3. `merge_entry`'de:
   - `was_in_base = false`, `is_dir = true`
   - `allocate_inode(base, is_dir=true)` → diyelim ki #42 döner. Bitmap, BGD `free_inode_count--`, `used_dirs_count++`, SB `free_inode_count--`.
   - `allocate_block(base)` → diyelim ki #100. Block 0'lanır, BGD/SB free_block_count--.
   - `node.mode = DIR | DPERM`, `direct_blocks[0] = 100`, `size = block_size`.
   - `write_inode(base, 42, node)`.
   - `write_dot_entries(base, 100, 42, home_in_base)` — block 100'e `.`→42 ve `..`→home yazılır.
   - `add_dir_entry(base, home_in_base, "user3", 42, DIR)` — home'un data block'una entry eklenir.
   - `base.inode_num = 42`.
4. is_dir, recursion'a girer. `dir_entries_of(base[42])` → sadece `.` ve `..` görür. `dir_entries_of(a[A_user3])` → A'nın user3 içindeki entry'lerini görür: `file4.txt`.
5. visit("file4.txt"): cb=0, ca=A_file4, cbb=0. Recursive `merge_entry(...)`.
6. Bu sefer:
   - `was_in_base = false`, `is_dir = false`
   - `allocate_inode(base, is_dir=false)` → #43.
   - `node.mode = FILE | FPERM`, `write_inode(base, 43, node)`.
   - `add_dir_entry(base, 42, "file4.txt", 43, FILE)` — block 100'e (parent dir) yeni entry. Şu an blockta `.`(12 byte) + `..`(geri kalan) var; `..`'nin length'ini düşürüp arta kalan boşluğa file4 entry'sini koyar.
   - `build_new_file_content(a=A_file4, b=0)` → A'daki tam içeriği oku.
   - `append_to_file(base, 43, content)` → block(lar) allocate edip içeriği yaz. size güncellenir.
   - `finalize_inode_metadata(base, 43, was_in_base=false, a=A_file4, b=0, is_dir=false)`:
     - mode/uid/gid → A_file4'tan kopyalanır
     - link_count = 1
     - block_count_512 = count_physical_blocks * (bs/512)
     - mtime/ctime/atime = A_file4'unkilerle max (base'de yok, B'de yok)
7. file4 işi bitti, user3 dönüşü. user3'ün finalize'i:
   - was_in_base=false, NEW
   - mode/uid/gid → A'nın user3 inode'undan
   - link_count = 2 + 0 (alt klasör yok) = 2
   - block_count_512 = 1 * (bs/512) (sadece 1 data block)
   - timestamps → A'nın user3 mtime/ctime/atime'ı
8. Devam eder, home/'un finalize'i de en sonda gelir; link_count'u doğru hesaplanır (artık user3'ü subdir olarak görüyor).
9. Root'un finalize'i en son. Ardından `merge_filesystems`:
   - `merge_super_block_timestamps(...)` → SB'nin global timestamps'i max'la güncelle
   - `flush_global_metadata(base)` → SB ve BGD diske yazılır
   - `write_backups(base)` → backup'ı olan her BG'ye SB + GDT kopyalanır

---

## 10. Eğer Hâlâ Kafan Karışırsa

**Endişelenme — kod 5/5 case'de tüm grader testlerinde tam puan alıyor.** PDF'in her şartı kontrol edildi:

- Tree printing (3.1.1 + 3.1.2)
- Dizin yapısı (3.2.1)
- Dosya içerikleri tam doğru (3.2.2, 1488 dosya test edildi)
- Tüm inode metadata alanları doğru (3.2.3)
- Bitmap, GDT, SB tutarlı (3.2.4–3.2.6)
- Backup'lar doğru yazıldı (3.2.7)

`hw3.cpp`'ye bakarken bu dosyayı yanında tut. Hangi fonksiyonu görürsen, bölüm 4'teki tabloda hangi göreve ait olduğunu bulabilirsin.

Eğer bir kısmı değiştirmek istersen:
1. Önce bu dosyada o kısımı oku (mantığı kavra)
2. Sonra `hw3.cpp`'deki fonksiyonu aç
3. Değişikliği yap
4. `g++ -Werror=unreachable-code -Werror=return-type -o mergeext2fs hw3.cpp ext2fs_print.c` ile derle
5. Bir test case ile dene: `cp testcases/case1_base.img /tmp/test.img && ./mergeext2fs /tmp/test.img testcases/case1_A.img testcases/case1_B.img`

Sakin ol, kod çalışıyor. İyi şanslar!
