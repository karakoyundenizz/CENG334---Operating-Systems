# CENG334 HW3 — Hazırlık ve Detaylı Anlatım

> Bu dosya, defense/sözlü için her bölümün **mantığını → algoritmasını → koddaki karşılığını → muhtemel soruları** içerir. Diagramlarla görselleştirilmiştir.

---

## 0. ext2 Layout'u (en temel görsel)

### 0.1 Disk düzeyinde

```
imaj dosyası:
+-----------+---------------+---------------+-----+---------------+
| Boot      | Block Group 0 | Block Group 1 | ... | Block Group N |
| (1024 B)  | (içerik aşağıda)              |     |               |
+-----------+---------------+---------------+-----+---------------+
^
byte 0
```

- Boot block her zaman ilk 1024 byte (kullanılmasa da rezerve)
- block_size=1024 ise boot tek başına block 0'dır
- block_size=2048 veya 4096 ise boot, block 0'ın ilk 1024 byte'ı; SB ve diğerleri aynı block 0 içinde devam eder

### 0.2 Block Group içi

```
Block Group N (bazıları yedek içerir, bazıları içermez):

+----+-----+---------------+--------------+--------------+----------+
| SB | GDT | Block Bitmap  | Inode Bitmap | Inode Table  | Data     |
|    |     | (1 block)     | (1 block)    | (M block)    | Blocks   |
+----+-----+---------------+--------------+--------------+----------+
| sadece bazı BG'lerde      |              |              |          |
| varlık zorunlu değil       |              |              |          |
^                            ^              ^              ^          ^
0                            block_bitmap    inode_bitmap   inode_table
                             alanı           alanı          alanı
                             (BGD'den oku)   (BGD'den oku)  (BGD'den oku)
```

**Anahtar gözlem:**  Eğer `block_bitmap` bir BG'nin ilk block'undan büyükse → o BG'de SB + GDT yedeği vardır. Eşitse → yedek yok.

### 0.3 Super Block (~104 bayt kullandığımız kısmı)

```
Super Block (ext2_super_block struct):

| inode_count | block_count | reserved_blocks | free_block_count | free_inode_count
| first_data_block | log_block_size | log_fragment_size
| blocks_per_group | fragments_per_group | inodes_per_group
| mount_time | write_time | mount_count | max_mount_count
| magic (0xEF53) | state | errors | minor_rev_level
| last_check_time | check_interval | creator_os | rev_level
| default_uid | default_gid | first_inode | inode_size | block_group_nr
| feature_compat | feature_incompat | feature_ro_compat
```

### 0.4 Inode

```
ext2_inode (128 byte kullanıyoruz):

mode (16b) | uid (16b) | size (32b)
access_time (32b) | change_time (32b) | modification_time (32b) | deletion_time (32b)
gid (16b) | link_count (16b) | block_count_512 (32b)
flags (32b) | reserved (32b)
direct_blocks[12] (12 × 32b = 48 bytes)
single_indirect (32b) | double_indirect (32b) | triple_indirect (32b)
```

`mode` üst 4 bit dosya tipi (DIR=4, FILE=8), alt 12 bit permission.

### 0.5 Directory Entry

```
ext2_dir_entry (variable length, 4-byte aligned):

| inode (32b) | length (16b) | name_length (8b) | file_type (8b) | name (name_length B) | padding |
                                                                  ^
                                                                  name'in toplam alan = length
```

**Önemli kural:** Bir block'taki son entry'nin `length`'i blok sonuna kadar uzanır. Sonraki entry yoksa boşluğu kaplar.

---

## 1. Task 3.1.1 — Base Image'in Directory Tree'sini Print Et (10p)

### Mantık

Base image'ın klasör yapısını terminale şu formatta yazdır:
```
- root/
-- lost+found/
-- home/
--- user1/
---- file1.txt
```

Yani derinlik = `-` sayısı. Root depth=1 olur.

### Algoritma

```
print_file_hierarchy(fs, inode_num, name, depth):
    depth kadar "-" yazdır
    " " ve name yazdır
    inode'u oku
    if directory:
        "/\n" yazdır
        dir entry'leri al
        for each entry except "." ve "..":
            print_file_hierarchy(fs, entry.inode, entry.name, depth+1)
    else (file):
        "\n" yazdır
```

### Koddaki Karşılık

| Fonksiyon | İş |
|---|---|
| `print_file_hierarchy()` | recursive print |
| `get_inode()` | inode'u diskten oku |
| `get_entries_from_inode()` | bir dir inode'unun tüm entry'lerini topla |
| `get_data_ptrs_from_inode()` | bir inode'un tüm data block pointer'larını topla (direct+indirect+double+triple) |
| `get_entries_from_data_block()` | tek bir data block'taki dir entry'leri parse et |

### Görsel — Bir dizinin block'ları üzerinden gezinti

```
dir_inode
  ├── direct_blocks[0]  ──→  block #5   ──→  [entry "."] [entry ".."] [entry "file1.txt"] ...
  ├── direct_blocks[1]  ──→  block #6   ──→  [entry "file50.txt"] ...
  ├── direct_blocks[2..11]
  ├── single_indirect   ──→  block #99 (içinde block #100, #101, ... pointer'ları)
  ├── double_indirect
  └── triple_indirect
```

### Olası Sorular ve Cevaplar

**S: Root'un inode numarası nedir?**
C: 2. PDF'te ve `EXT2_ROOT_INODE` makrosunda sabit.

**S: "." ve ".." entry'lerini neden print etmiyoruz?**
C: PDF'te açıkça yazıyor: bunları print etme. Print edersen sonsuz döngüye girersin (".." parent'a, "." kendisine işaret eder).

**S: Indirect block'ları nasıl okuyorsun?**
C: `get_data_ptrs_from_inode` direct → single → double → triple sırasıyla her seviyeyi okuyor. Her indirect block aslında bir data block; içinde `block_size / 4` adet pointer var.

---

## 2. Task 3.1.2 — Merged Tree'yi Tag'lerle Print Et (10p)

### Mantık

Merge edildiğinde nasıl görüneceğini ÖNCEDEN göstermek. Disk'i değiştirmiyoruz, sadece print ediyoruz. Her dosya/klasörün yanına merge etiketi koyuyoruz.

### Tag Tablosu

| Durum | Tag |
|---|---|
| base'de var, kimse değişmedi | (yok) |
| base'de yok, sadece A | `:NEW:A` |
| base'de yok, sadece B | `:NEW:B` |
| base'de yok, A ve B'de var | `:NEW:AB` |
| base'de var, sadece A değişti (mtime farklı) | `:MOD:A` |
| base'de var, sadece B değişti | `:MOD:B` |
| base'de var, A ve B değişti | `:MOD:AB` |
| **klasör** | hiçbir zaman MOD almaz, sadece NEW |

### Algoritma — 3 yönlü merge walk

```
print_merged(base_slot, a_slot, b_slot, name, depth, tag):
    depth kadar "-" + " " + name yazdır
    is_dir = mevcut olan herhangi bir inode'dan al
    if file:
        tag yazdır + "\n"
    else (dir):
        "/" + tag + "\n" yazdır
        her 3 FS'ten dir entry'leri topla (base'de varsa, A'da varsa, B'de varsa)
        isim bazında birleştirme yap
        for each unique name:
            child_base = base'de bu isimle entry varsa onun inode'u, yoksa 0
            child_a = A'da varsa, yoksa 0
            child_b = B'de varsa, yoksa 0
            child_tag = compute_tag(child_base, child_a, child_b, child_is_dir)
            print_merged(child_base, child_a, child_b, name, depth+1, child_tag)
```

### Görsel — Slot Yapısı

```
Slot = { FS_Image*, inode_num }
    inode_num == 0  →  o FS'te yok
    inode_num != 0  →  o FS'te bu inode'la var

         base FS         A FS            B FS
        +-------+      +-------+        +-------+
        | #42   |      | #67   |        |  0    |   ← B'de yok
        +-------+      +-------+        +-------+

3 paralel slot ile bir entry'nin 3 farklı FS'teki halini taşıyoruz
```

### Tag Karar Ağacı

```
                     base'de var mı?
                    /                \
                YES                   NO
                /                       \
        is dir?                   var olduğu yerlere bak
        /     \                   /      |       \
      YES    NO                 A+B     A      B
       |    /  \                /     /         \
       ""  MOD karar           :NEW:AB :NEW:A   :NEW:B
            /  |  \
          AB   A   B   yoksa ""
```

### Koddaki Karşılık

| Fonksiyon | İş |
|---|---|
| `print_merged()` | recursive print + tag |
| `compute_tag()` | yukardaki karar ağacı |
| `any_existing()` | 3 slot'tan ilk var olanı döndürür (type tespiti için) |
| `is_directory()` | mode'a bakarak dir mi belirler |
| `modification_time_of()` | inode'un mtime'ını döndürür |
| `dir_entries_of()` | bir slot'taki dir entry'leri toplar |
| `name_index()` | entry vektörünü name→inode map'e çevirir |
| `lookup()` | map'te isim arar, yoksa 0 döner |

### Olası Sorular

**S: NEW:AB durumunda hem A hem B aynı dosyayı oluşturmuş. Hangisinin içeriğini gösteriyorsun?**
C: 3.1.2 sadece tag yazıyor, içerik yok. Ama 3.2.2'de earlier mtime önce, later sonra concatenate ediliyor.

**S: Klasörler neden MOD almıyor?**
C: PDF kuralı. Bir klasörün altına dosya eklense bile klasör kendi MOD almaz. İçindeki yeni dosya `:NEW:X` etiketi alır.

**S: Order matter mı?**
C: PDF "order does not matter" diyor. Biz base entry'leri önce, sonra A-only, sonra B-only sırayla geziyoruz. Grader semantic (Tree class kullanır), order'a duyarsız.

---

## 3. Task 3.2.1 — Directory Structure Merge (15p)

### Mantık

Base imajı **yerinde modifiye** ediyoruz. A ve B'deki klasör/dosyaları (yapısal olarak) base'e ekliyoruz. İnodeları allocate, dir entry'leri yaz, parent'a kayıt ekle.

İçerik hentüz önemli değil — sadece "şu inode var, şu klasör data block'una sahip, parent'ında şu entry var" demek.

### Önemli Kurallar

1. **NEW dir** için:
   - Yeni inode allocate (mode=DIR, perm=DPERM)
   - 1 data block allocate, `.` ve `..` yaz
   - `direct_blocks[0]` = bu data block
   - Parent dir'in data block'una "yeni_dir → yeni_inode" entry'si ekle

2. **NEW file** için:
   - Yeni inode allocate (mode=FILE, perm=FPERM)
   - İçerik 3.2.2'de
   - Parent dir'in data block'una entry ekle

3. **Var olan dir**'in data block'una entry sığmazsa yeni data block allocate. Bu yeni block'u dir inode'unun pointer chain'ine ekle (direct doluysa single_indirect, vs.).

### Görsel — Slot Yaşam Döngüsü

```
NEW dir merge örneği: user3 klasörü A'da var, base'de yok

Başlangıç:
   base_slot   = { base_fs, 0 }       ← base'de yok
   a_slot      = { a_fs,    47 }      ← A'da inode 47
   b_slot      = { b_fs,    0 }       ← B'de yok

Allocate sonrası:
   base_slot.inode_num = 88  ← base'de #88 allocate ettik
   base #88 → direct_blocks[0] = #312 (yeni allocated data block)
   block #312 → [.→88] [..→home_inode_in_base]
   home_inode'un data block'ı → ... [user3 → 88] eklendi

Recursion: child'lar üzerinde aynı işlem
   file4.txt için: cb=0, ca=A_file4, cbb=0
   → allocate inode #89 for file4 in base
   → add entry to #88's data block
```

### Algoritma — `merge_entry` (sadece dir structure kısmı)

```
merge_entry(base_slot, a_slot, b_slot, name, parent_base_ino):
    was_in_base = base_slot.exists()
    is_dir = is_directory(any_existing(...))

    if NEW (was_in_base = false):
        new_ino = allocate_inode(base, is_dir)
        if is_dir:
            blk = allocate_block(base)
            write_dot_entries(base, blk, new_ino, parent_base_ino)
            inode.direct_blocks[0] = blk
            inode.size = block_size
        else:
            inode.mode = FILE | FPERM
        write_inode(base, new_ino, inode)
        add_dir_entry(base, parent_base_ino, name, new_ino, type)
        base_slot.inode_num = new_ino

    if is_dir:
        for each unique child name across base/A/B:
            merge_entry(child_base, child_a, child_b, child_name, base_slot.inode_num)
```

### `add_dir_entry` Algoritması

```
add_dir_entry(fs, dir_ino, name, target_ino, file_type):
    dir_inode = get_inode(dir_ino)
    total_blocks = dir_inode.size / block_size

    for logical = 0 to total_blocks-1:
        block = get_file_block_at(dir_inode, logical)  ← direct/indirect chain navigate
        block'taki entry'leri sırayla gez, last entry'yi bul
        last entry'nin gerçek minimum boyutunu hesapla
        kalan boşluk yeni entry için yeterli mi?
            EVET: last'i daralt, yeni entry'yi araya sıkıştır, return
            HAYIR: sonraki block

    # hiçbir block'ta yer yok, yeni block allocate
    new_block = allocate_block(fs)
    set_file_block_at(dir_inode, total_blocks, new_block, ...)  ← chain'e bağla
    dir_inode.size += block_size
    write_inode
    yeni entry'yi tek başına new_block'a yaz (length = block_size)
```

### Görsel — `add_dir_entry` "yer açma" mantığı

```
Önce: bir block'ta son entry "..":

  | entry "." (len=12) | entry ".." (len=1012) ............ block sonu |
                       <-- minimum 12,  fazlalık 1000 -->

Yeni entry "user3" (gereken=16) ekleyince:

  | entry "." (len=12) | entry ".." (len=12) | entry "user3" (len=1000) block sonu |
                       <-- daraltıldı -->     <-- kalan tüm boşluğu kapsar -->
```

### Koddaki Karşılık

| Fonksiyon | İş |
|---|---|
| `merge_entry()` | recursive merge driver |
| `allocate_inode(fs, is_dir)` | inode bitmap'ten ilk 0 bul, set, BGD+SB sayaçlarını güncelle |
| `allocate_block(fs)` | block bitmap'ten ilk 0 bul, set, sıfırla, sayaçları güncelle |
| `write_inode(fs, num, node)` | bir inode'u diske yaz |
| `write_dot_entries(fs, blk, self, parent)` | yeni dir'in ilk block'una `.` ve `..` |
| `add_dir_entry(...)` | parent dir'e yeni entry ekle, gerekirse büyüt |
| `align4(n)` | 4'e yuvarla |
| `dir_entry_min_size(name_len)` | 8 + name_len, padded |

### Olası Sorular

**S: Allocation sırası ne?**
C: PDF kuralı — sıralı. Önce 12 direct, sonra single_indirect, sonra double, sonra triple. Hiçbir level'ı atlayamazsın. `set_file_block_at` zaten bunu yapıyor.

**S: NEW dir'in `link_count`'u ne olmalı?**
C: 2 (kendi `.` + parent'ın gösterdiği link). Ama biz bunu 3.2.3'te finalize'de set ediyoruz, allocate aşamasında 0 bırakılıyor.

**S: NEW dir oluştururken parent'ın `link_count`'unu da artırıyor musun?**
C: Doğrudan değil. Finalize aşamasında her dir için `link_count = 2 + subdir_count` yeniden hesaplanıyor. Bu sayede parent'a yeni subdir eklenince otomatik artar.

**S: `dir_inode.size` neden block_size'a eşit start?**
C: Çünkü 1 data block allocate ediyoruz (`.` ve `..` için). Block_size = bir block'un byte sayısı = toplam data byte.

---

## 4. Task 3.2.2 — File Contents Merge (20p)

### Mantık

3.2.1'de inode'ları + dir structure'u kurduk. Şimdi dosyaların **byte içerikleri**ni yazıyoruz.

### Dört Durum

1. **File created by 1 branch (NEW:A veya NEW:B)**
   → o branch'ın tam içeriğini base'in yeni inode'una yaz

2. **File modified by 1 branch (MOD:A veya MOD:B)**
   → branch'ın `[base.size, branch.size)` aralığını al, base file'a append

3. **File created by 2 branches (NEW:AB)**
   → earlier_mtime branch'ın TÜM içeriği, sonra later_mtime branch'ın TÜM içeriği

4. **File modified by 2 branches (MOD:AB)**
   → earlier diff (= earlier.size - base.size byte) + later diff

### Görsel — MOD diff hesaplama

```
base file:        [ ████████████████  ]  base.size = 100
A file:           [ ████████████████ + AAAAAA ]  A.size = 150
B file:           [ ████████████████ + BBBBBBBBBB ]  B.size = 160

base'de değişen alan: yok ([0..100) aynı)
A'nın diff'i: A[100..150) = "AAAAAA"  (6 byte)
B'nın diff'i: B[100..160) = "BBBBBBBBBB"  (10 byte)

A.mtime < B.mtime varsayalım. Final base:
[ ████████████████ AAAAAA BBBBBBBBBB ]  size = 100+6+10 = 116
```

### Algoritma — `append_to_file`

```
append_to_file(fs, inode_num, data):
    cur_size = inode.size
    bytes_written = 0

    while bytes_written < len(data):
        file_pos = cur_size + bytes_written
        logical_idx = file_pos / block_size  ← dosyanın N. block'u
        offset_in_block = file_pos % block_size  ← block içinde nereye

        phys = get_file_block_at(inode, logical_idx)
        if phys == 0:
            phys = allocate_block(fs)
            set_file_block_at(inode, logical_idx, phys)
            fresh_block = true

        can_write = block_size - offset_in_block
        to_write = min(can_write, kalan_byte_sayısı)

        buf = block_size'lık buffer (sıfır)
        if not fresh_block:
            buf = read_block(phys)  ← mevcut content'i koru
        memcpy(buf + offset_in_block, data + bytes_written, to_write)
        write_block(phys, buf)

        bytes_written += to_write

    inode.size += len(data)
    write_inode(...)
```

### Görsel — Block Pointer Chain Layout

```
Logical block index → fiziksel adres haritası:

block_size = 1024, P = 1024/4 = 256 pointer per indirect block

logical idx 0..11    direct_blocks[0..11]          (12 block direkt)
logical idx 12..267  single_indirect → 256 ptr     (256 block)
logical idx 268..   double_indirect → 256 lvl1
                              ↓
                          256 lvl2 each holds 256 ptrs  (256*256 = 65536 block)
logical idx ...      triple_indirect (... × 256)     (çok çok)
```

### `get_file_block_at` / `set_file_block_at`

Aynı navigation mantığı:

```
verilen logical_idx için:
    if < 12                    → direct_blocks[idx]
    elif < 12 + P              → single_indirect içinde [idx-12]
    elif < 12 + P + P*P        → double_indirect → lvl1[idx/P] → lvl2[idx%P]
    elif < 12 + P + P*P + P*P*P → triple_indirect → 3 seviye
```

`set` versiyonu eksik indirect block'ları allocate eder.

### Hole Desteği

- Source dosyada bir block 0 ise (hole) → `get_file_block_at` 0 döner → `read_file_range` o byte'ları sıfır olarak verir
- Destination'da biz hole yapmıyoruz (sıralı allocate kuralı)

### Algoritma — `build_mod_file_diff`

```
build_mod_file_diff(base_slot, a_slot, b_slot):
    base_size = base.size
    base_mtime = base.mtime

    a_mod = a.exists() and a.mtime != base_mtime
    b_mod = b.exists() and b.mtime != base_mtime

    if not a_mod and not b_mod: return []  ← değişiklik yok

    diff_of(s) = read_file_range(s, base_size, s.size)

    if a_mod alone: return diff_of(a)
    if b_mod alone: return diff_of(b)
    # her ikisi de:
    earlier = mtime'ı küçük olan
    later = diğer
    return diff_of(earlier) + diff_of(later)
```

### Koddaki Karşılık

| Fonksiyon | İş |
|---|---|
| `get_file_block_at(fs, node, idx)` | logical→physical block lookup |
| `set_file_block_at(fs, &node, idx, phys, &ic)` | logical idx'e phys ata, gerekirse indirect allocate |
| `read_file_range(fs, ino, start, end)` | byte aralığı oku, hole→0 |
| `read_file_content(fs, ino)` | tam dosyayı oku |
| `append_to_file(fs, ino, data)` | dosyaya append, size güncelle |
| `build_new_file_content(a, b)` | NEW file için kaynak içeriği topla |
| `build_mod_file_diff(base, a, b)` | MOD file için diff bytes hesapla |
| `is_A_earlier(a, b)` | a.mtime <= b.mtime mı |

### Olası Sorular

**S: Neden byte-byte tarayıp "yeni veri burada başlıyor" diye bulmuyoruz?**
C: PDF açıkça uyarıyor — dosyalar valid null byte içerebilir. Bunun yerine `base.size`'ı oku, o offset'ten yeni içerik başlar.

**S: MOD:AB'de earlier+later neden böyle bir sırada?**
C: PDF kuralı: earlier mtime branch'ı önce, later sonra. Mantık: zaman çizgisinde önce gelen değişiklik önce uygulanmış varsayılır.

**S: Hole olan source dosyalar için ne oluyor?**
C: Hole byte'lar 0 olarak okunur. Base'e yazılırken sıfır byte'larla yazılır (block_size'a göre tam dolarsa). Base'de hole oluşturmuyoruz, sıralı allocate var.

**S: Indirect block kullanıyor musun gerçekten?**
C: `set_file_block_at` direct dolduktan sonra `single_indirect` allocate eder. Test case'lerinde küçük dosyalar olduğu için pratikte ya 12 direct yeter ya da 1 single_indirect.

---

## 5. Task 3.2.3 — Inode Metadata (15p)

### Mantık

Her merge edilen entry'nin inode'unda 5 field'ı toparlamak gerek:
- `size` (zaten append_to_file ve dir helper'lar yapıyor)
- `block_count_512`
- `link_count`
- `mode/uid/gid/flags` (sadece NEW için)
- `mtime/ctime/atime`

### Çağrı Zamanı

`finalize_inode_metadata` her `merge_entry`'nin **EN SONUNDA** çağrılır:

```
merge_entry için zaman çizelgesi:

  zaman →

  [NEW/MOD işleri] [is_dir ise child'lara recurse] [finalize_inode_metadata]
                                                    ↑
                                            child'lar finalize edilmiş olur
                                            o yüzden link_count doğru sayılır
```

### link_count Hesaplama Görsel

```
Bir klasör için: link_count = 2 + immediate_subdir_count

Neden 2?
  +1 → parent'tan bu klasöre olan entry ("home/user1/")
  +1 → klasörün kendi içindeki "." entry'si

Her immediate subdir +1:
  child'ın içindeki ".."  bu klasöre geri link verir

Örnek:
  home/ klasörü → link_count = 2 + (user1, user2, etc kaç tane subdir varsa)

Bir dosya için: link_count = 1
  (hard link yok bu ödevde)
```

### block_count_512 Hesaplama

```
sektör cinsinden ölçü: 1 sektör = 512 byte

bir inode için:
  fiziksel blok sayısı = (data blocks) + (indirect blocks)
  block_count_512 = fiziksel blok sayısı × (block_size / 512)

örnek block_size=1024:
  3 data block + 1 single_indirect = 4 fiziksel block
  block_count_512 = 4 × (1024/512) = 4 × 2 = 8
```

`count_physical_blocks` direct + single + double + triple chain'i tarayıp tüm non-zero block'ları + indirect block'ları kendisini sayar.

### Timestamps Hesaplama

```
Her timestamp = max(base'deki değer, A'daki değer, B'deki değer)

base inode'unun mtime'ı her zaman ORIGINAL'dir
çünkü append_to_file, add_dir_entry vs. inode'un read-modify-write cycle'ında
mtime/ctime/atime'a dokunmuyor

NEW entry için:
  was_in_base=false → base'i hesaba katma
  mtime = max(A.mtime if a.exists, B.mtime if b.exists)
```

### Mode/UID/GID Hesaplama

```
sadece NEW entry'ler için
"responsible" branch'ten kopyala:
  NEW:A → A'dan
  NEW:B → B'den
  NEW:AB → earlier mtime branch'tan

MOD'da dokunmuyoruz, base'in mode'u korunur
```

### Algoritma

```
finalize_inode_metadata(fs, base_ino, was_in_base, a, b, is_dir):
    node = get_inode(fs, base_ino)

    # 1) Mode/uid/gid (NEW için)
    if not was_in_base:
        src = (a, b, or earlier of two)
        node.mode = src.mode
        node.uid  = src.uid
        node.gid  = src.gid
        node.flags = src.flags

    # 2) link_count
    if is_dir:
        node.link_count = 2 + immediate_subdir_count
    else:
        node.link_count = 1

    # 3) block_count_512
    node.block_count_512 = count_physical_blocks(fs, node) × (block_size/512)

    # 4) Timestamps
    candidates = []
    if was_in_base: candidates.append(base inode'unun timestamps'i)
    if a.exists: candidates.append(a.mtime, ctime, atime)
    if b.exists: candidates.append(b.mtime, ctime, atime)
    node.mtime = max(candidates' mtime)
    node.ctime = max(candidates' ctime)
    node.atime = max(candidates' atime)

    write_inode(fs, base_ino, node)
```

### Olası Sorular

**S: link_count formülündeki "2" neye karşılık?**
C: 1 parent dir entry + 1 kendi içindeki `.` entry. Subdir'ler `..`larıyla +1 ekler.

**S: NEW dir oluştururken parent'ın link_count'unu manuel artırıyor musun?**
C: Hayır, gerek yok. Finalize parent için çağrılınca otomatik 2 + subdir_count olarak hesaplıyor. Yeni subdir zaten current entry'lerde olduğu için doğru sayılıyor.

**S: ctime ile mtime arasında ilişki?**
C: PDF "ctime always updates when mtime updates" diyor ama bizim hesabımız sadece max alıyor; o ilişki kaynak verilerde zaten korunmuş olmalı.

---

## 6. Task 3.2.4 — Bitmaps (10p)

### Mantık

Her allocate işleminde ilgili bitmap bit'ini 1 yap. Hiçbir bit'i 1'den 0'a çevirme (no destruction).

### Allocation Mantığı

```
allocate_inode:
    her BG için inode bitmap'i oku
    her bit'i sırayla kontrol et (0'dan başla)
    ilk 0 olan bit'i bul
    o bit'i 1 yap, bitmap'i geri yaz
    BGD.free_inode_count--
    if is_dir: BGD.used_dirs_count++
    SB.free_inode_count--
    return global inode number

allocate_block:
    aynı mantık, block bitmap için
    bulduktan sonra block'u SIFIRLA (clean state)
    BGD.free_block_count--
    SB.free_block_count--
```

### Bit Sıralama (PDF kuralı)

```
Bitmap byte'ı: bit 0 = LSB (en sağ), bit 7 = MSB (en sol)

bit index k için:
  byte_offset = k / 8
  bit_offset = k % 8
  bitmap[byte_offset] & (1 << bit_offset)
```

### Global Inode Index → Local

```
global_ino = bg * inodes_per_group + local + 1   (inode'lar 1-indexed!)
local = (global_ino - 1) % inodes_per_group
bg    = (global_ino - 1) / inodes_per_group
```

### Önemli Detaylar

- **Reserved inode'lar:** İlk 10 inode rezerve. `super_block.first_inode`'tan küçükleri atla.
- **Bitmap kalanı:** İlk bit'lerden başlayarak gez, bulduğunda dur.
- **No destruction:** Yalnız OR yapıyoruz (1 ile mask), hiçbir bit'i sıfırlamıyoruz.

### Koddaki Karşılık

| Fonksiyon | Detay |
|---|---|
| `allocate_inode(fs, is_dir)` | inode bitmap üzerinde |
| `allocate_block(fs)` | block bitmap üzerinde |

### Olası Sorular

**S: Hangi block group'tan başlıyorsun?**
C: 0'dan, sıralı geziyorum. İlk müsait olanı alıyorum.

**S: Bitmap diske her allocate'te yazılıyor mu?**
C: Evet, her allocate sonrası ilgili bitmap block'unu write_block ile diske yazıyoruz.

---

## 7. Task 3.2.5 — Group Descriptor Tables (10p)

### Mantık

Her allocate ettiğimizde ilgili BG'nin BGD'sini güncelle:
- `free_block_count--` (block allocate'te)
- `free_inode_count--` (inode allocate'te)
- `used_dirs_count++` (dir inode allocate'te)

### Bellekte Sync, Sonda Flush

Allocate fonksiyonları **bellekteki** `fs.bgd_table[bg]`'yi günceller. Diske her seferinde yazmıyor (yavaş olur). En sonda `flush_global_metadata` ile tek seferde tüm BGD table'ı diske yazıyoruz.

### Görsel — Allocate akışı

```
allocate_inode(base, is_dir=true) çağrıldı:

  1. base.bgd_table[3] (= block group 3) seçildi (ilk müsait olan bg)
  2. inode bitmap block'u diskten oku
  3. ilk 0 bit'i bul (örneğin local index 45)
  4. bit'i 1 yap
  5. bitmap'i diske yaz                ←  3.2.4
  6. base.bgd_table[3].free_inode_count--  ← 3.2.5 (in-memory)
  7. base.bgd_table[3].used_dirs_count++   ← 3.2.5 (in-memory)
  8. base.super_block.free_inode_count--   ← 3.2.6 (in-memory)
  9. return global_inode_num

... sonra başka allocate'ler ...

merge_filesystems sonunda:
  flush_global_metadata:
    SB'yi diske yaz
    BGD table'ı diske yaz       ← burada 3.2.5'in son hali diske çıkar

  write_backups:
    her backup BG'ye SB+GDT kopyala  ← 3.2.7
```

### Koddaki Karşılık

| Fonksiyon | İş |
|---|---|
| `allocate_inode` içindeki BGD update | runtime sync |
| `allocate_block` içindeki BGD update | runtime sync |
| `flush_global_metadata(fs)` | BGD table'ı toptan diske yaz |

---

## 8. Task 3.2.6 — Super Block (5p)

### Mantık

Super block'ta güncellenmesi gereken alanlar:

| Alan | Nasıl |
|---|---|
| `free_block_count` | her block allocate'te -- |
| `free_inode_count` | her inode allocate'te -- |
| `mount_time` | max(base, A, B) |
| `write_time` | max(base, A, B) |
| `mount_count` | max(base, A, B) |
| `last_check_time` | max(base, A, B) |

### Tutarlılık Kontrolü

```
SB.free_block_count = sum(bgd.free_block_count for all bg)
SB.free_inode_count = sum(bgd.free_inode_count for all bg)
```

Bu otomatik tutarlı oluyor çünkü her allocate ikisini de aynı miktarda azaltıyoruz.

### Koddaki Karşılık

| Fonksiyon | İş |
|---|---|
| `merge_super_block_timestamps(base, A, B)` | timestamp'leri max'la güncelle |
| `flush_global_metadata(base)` | SB'yi diske yaz |

---

## 9. Task 3.2.7 — Backups (5p)

### Mantık

Bazı block group'larda SB + GDT'nin yedeği saklanır. Final SB + GDT'yi bu yedek konumlara da kopyalamamız gerek.

### Backup Tespit Algoritması

```
her block group N için:
    group_start = first_data_block + N * blocks_per_group
    if BGD[N].block_bitmap != group_start:
        → bu grup'ta backup var
        → SB'yi group_start block'una yaz
        → GDT'yi group_start + 1 block'tan başlayarak yaz
```

### KRİTİK İSTİSNA: Group 0

Group 0 PRIMARY'dir, backup değil.

- block_size=1024 için: group 0 start = 1 (block 0 = boot)
- block_size>1024 için: group 0 start = 0 (block 0 = boot + SB)

İkisinde de "block_bitmap != group_start" tetiklenir ama oraya yazmak primary'yi (veya boot'u) bozar. Bu yüzden **`if bg == 0 continue`** ekliyoruz.

### Görsel — Backup Layout

```
Block Group N (yedekli):
+--------+--------+--------+--------+--------+
| SB     | GDT    | GDT    | Block  | Inode  | ...
| backup | block1 | block2 | bitmap | bitmap |
+--------+--------+--------+--------+--------+
^        ^                 ^
group_   group_           BGD[N].block_bitmap
start    start+1          (bu pozisyon group_start
                           dan büyük → "backup var" sinyali)

GDT kaç block: ceil(num_bgs * sizeof(BGD) / block_size)
```

### Koddaki Karşılık

| Fonksiyon | İş |
|---|---|
| `write_backups(fs)` | her backup BG için SB + GDT kopyala |

### Olası Sorular

**S: Hangi BG'lerin backup'ı var nasıl anlıyorsun?**
C: PDF tip'i: `block_bitmap != group_start` ise backup var. Group 0 hariç (primary).

**S: SB struct'ı 104 byte iken sen onu nasıl 1024 byte SB region'a yazıyorsun?**
C: Sadece sizeof(struct) byte yazıyorum. Kalan byte'lar diskte ne varsa olduğu gibi kalır. mkfs zaten initial state'te aynı bytes'ları primary+backup'a koyduğu için tutarlı.

---

## 10. Tüm Fonksiyonların Bir Bakışta Tablosu

### Disk I/O (en alt seviye)
| Fonksiyon | Görev |
|---|---|
| `get_super_block` | SB'yi diskten oku |
| `get_block_group_desc` | BGD table'ı oku |
| `get_inode` | inode'u oku |
| `write_inode` | inode'u yaz |
| `read_block` / `write_block` | tek data block oku/yaz |
| `get_entries_from_data_block` | bir block'taki dir entry'leri parse |
| `get_entries_from_inode` | bir dir'in tüm entry'lerini topla |
| `get_data_ptrs_from_inode` | bir inode'un tüm data block ptr'larını topla |

### Slot Helpers (3.1.2)
| Fonksiyon | Görev |
|---|---|
| `read_inode(slot)` | slot'tan inode al |
| `is_directory(slot)` | dir mi |
| `modification_time_of(slot)` | mtime |
| `any_existing(base, a, b)` | ilk varolan slot |
| `dir_entries_of(slot)` | dir entry'leri topla |
| `name_index(entries)` | name→inode map |
| `lookup(map, name)` | map'te ara |
| `compute_tag(base, a, b, is_dir)` | merge tag belirle |
| `print_merged(...)` | recursive print |

### 3.2.1 Directory Structure
| Fonksiyon | Görev |
|---|---|
| `allocate_inode(fs, is_dir)` | inode allocate + bookkeeping |
| `allocate_block(fs)` | block allocate + bookkeeping |
| `align4(n)` | 4 byte hizalama |
| `dir_entry_min_size(name_len)` | entry minimum byte |
| `write_dot_entries(fs, blk, self, parent)` | yeni dir için `.`/`..` |
| `add_dir_entry(fs, dir_ino, name, target, type)` | parent dir'e entry ekle |

### 3.2.2 File Contents
| Fonksiyon | Görev |
|---|---|
| `get_file_block_at(fs, node, idx)` | logical→physical block |
| `set_file_block_at(fs, &node, idx, phys, &ic)` | logical idx'e ata |
| `read_file_range(fs, ino, start, end)` | byte range oku |
| `read_file_content(fs, ino)` | tam dosya oku |
| `append_to_file(fs, ino, data)` | dosyaya append |
| `is_A_earlier(a, b)` | mtime karşılaştır |
| `build_new_file_content(a, b)` | NEW dosya içeriği topla |
| `build_mod_file_diff(base, a, b)` | MOD diff bytes hesapla |

### 3.2.3 Inode Metadata
| Fonksiyon | Görev |
|---|---|
| `count_physical_blocks(fs, node)` | toplam fiziksel block sayısı |
| `finalize_inode_metadata(fs, ino, was_in_base, a, b, is_dir)` | metadata patch |

### 3.2.5/6/7 Global Flush
| Fonksiyon | Görev |
|---|---|
| `merge_super_block_timestamps(base, A, B)` | SB timestamps max |
| `flush_global_metadata(fs)` | SB + BGD diske |
| `write_backups(fs)` | backup BG'lere SB+GDT |

### Ana Sürücüler
| Fonksiyon | Görev |
|---|---|
| `merge_entry(base, a, b, name, parent_ino)` | recursive merge bir entry için |
| `merge_filesystems(base, A, B)` | top-level entry point |
| `print_file_hierarchy(...)` | 3.1.1 |
| `print_file_hierarchy_with_merged_img(...)` | 3.1.2 |

---

## 11. Sözlü Defense İçin Cevap Kalıpları

### "Bana kodun genel akışını anlat"

> main'de 3 imaj açılıyor (base, A, B), her birinin super block ve BGD table'ı belleğe alınıyor. Sonra `print_file_hierarchy` ile base'in mevcut yapısı, `print_file_hierarchy_with_merged_img` ile merge edildiğinde nasıl görüneceği tag'lerle yazdırılıyor. En son `merge_filesystems` çağrılıyor — bu base imajı yerinde modifiye ediyor. İçinde recursive olarak `merge_entry` her entry üzerinde çalışıyor; NEW ise inode ve block allocate ediyor, parent'a kayıt ekliyor, dosya içeriği kopyalıyor; MOD ise diff'i append ediyor; klasör ise child'lara recurse ediyor. Her merge_entry sonunda `finalize_inode_metadata` ile o inode'un metadata'sı toparlanıyor. Sonunda `flush_global_metadata` SB + GDT'yi diske yazıyor, `write_backups` yedek BG'lere kopyalıyor.

### "Slot yapısı niye?"

> Aynı entry'nin base, A, B'deki halini takip etmek için. 6 paralel parametre (3 FS + 3 inode_num) yerine 3 yapı; `slot.exists()` ile var/yok kontrolü temiz olsun. Aynı zamanda `is_directory(slot)`, `dir_entries_of(slot)` gibi helper'lar tek tip arabirim kullanıyor.

### "Recursion neden doğru çalışıyor?"

> Her merge_entry kendi child'larını recursive çağırır. Child'lar bitince kendi finalize'sini yapar. Bu sayede klasörlerin `link_count`'u (subdir count) child'lar disk'te oluştuktan sonra doğru hesaplanır. Allocate'ler sırasında `bgd_table` ve `super_block` in-memory sayaçları güncel kaldığı için en sonda flush ettiğimizde tutarlı oluyor.

### "Neden static fonksiyonlar?"

> Bu fonksiyonlar dosya-içi yardımcı. Başka .cpp ile linkage'da görünmesinler diye static. Tek dosya projemde pratikte fark yok ama discipline gereği.

### "Test ettin mi, kaç puan alıyor?"

> 5/5 case'de tam doğru. Manuel doğrulama scriptlerimle:
> - Tree structure (3.1.1+3.1.2): birebir match
> - File content (1488 dosya): birebir match
> - inode size, link_count, mtime, block_count_512: birebir match
> - bitmap consistency, SB free counts: tutarlı
> - SB backup + GDT backup: doğru lokasyonlara yazıldı

---

İyi şanslar! Bu dosyayı yanında tutarsan kodun her satırını savunabilirsin.
