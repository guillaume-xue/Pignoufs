#ifndef UTILITAIRES_H
#define UTILITAIRES_H

#include "structures.h"

// Calcul SHA1 d'un bloc de données
static void calcul_sha1(const void *data, size_t len, uint8_t *out)
{
  SHA_CTX ctx;
  SHA1_Init(&ctx);
  SHA1_Update(&ctx, data, len);
  SHA1_Final(out, &ctx);
}

// Récupération des données du conteneur
static void get_conteneur_data(uint8_t *map, int32_t *nb1, int32_t *nbi, int32_t *nba, int32_t *nbb)
{
  struct pignoufs *sb = (struct pignoufs *)map;
  *nbi = le32toh(sb->nb_i);
  *nba = le32toh(sb->nb_a);
  *nbb = le32toh(sb->nb_b);
  *nb1 = *nbb - 1 - *nbi - *nba;./
}

static void decremente_lbl(uint8_t *map)
{
  struct pignoufs *sb = (struct pignoufs *)map;
  sb->nb_l--;
  calcul_sha1(map, 4000, map + 4000);
}

static void incremente_lbl(uint8_t *map)
{
  struct pignoufs *sb = (struct pignoufs *)map;
  sb->nb_l++;
  calcul_sha1(map, 4000, map + 4000);
}

// Initialisation d'un inode, fichier simple pour l'instant
static void init_inode(struct inode *in, const char *name, bool type)
{
  time_t now = time(NULL);
  in->flags = htole32((1 << 0)        /* existence */
                      | (1 << 1)      /* permission de lecture */
                      | (1 << 2)      /* permission d'écriture */
                      | (0 << 3)      /* verrouillage en lecture */
                      | (0 << 4)      /* verrouillage en écriture */
                      | (type << 5)); /* 0 fichier, 1 répertoire */
  in->file_size = htole32(0);
  in->creation_time = htole32(now);
  in->access_time = htole32(now);
  in->modification_time = htole32(now);
  strncpy(in->filename, name, sizeof(in->filename) - 1);
  in->filename[sizeof(in->filename) - 1] = '\0';
  for (int i = 0; i < 900; i++)
  {
    in->direct_blocks[i] = htole32(-1);
  }
  in->double_indirect_block = htole32(-1);
  memset(in->extensions, 0, sizeof in->extensions);
  calcul_sha1(in, 4000, in->sha1);
  in->type = htole32(3);
}

// Ouverture du système de fichiers et projection en mémoire
static int open_fs(const char *fsname, uint8_t **mapp, size_t *sizep)
{
  int fd = open(fsname, O_RDWR);
  if (fd < 0)
  {
    perror("open");
    return -1;
  }
  struct stat st;
  if (fstat(fd, &st) < 0)
  {
    perror("fstat");
    close(fd);
    return -1;
  }
  *sizep = st.st_size;
  *mapp = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (*mapp == MAP_FAILED)
  {
    perror("mmap");
    close(fd);
    return -1;
  }
  return fd;
}

// Fermeture du système de fichiers et synchronisation
static void close_fs(int fd, uint8_t *map, size_t size)
{
  msync(map, size, MS_SYNC);
  munmap(map, size);
  close(fd);
}

static void bitmap_alloc(uint8_t *map, int32_t blknum)
{
  int32_t idx = blknum / 32000;
  int32_t bit = blknum % 32000;
  uint8_t *data_position = map + (uint64_t)(1 + idx) * 4096;
  struct bitmap_block *bb = (struct bitmap_block *)data_position;
  bb->bits[bit / 8] &= ~(1 << (bit % 8));
  calcul_sha1(bb->bits, 4000, bb->sha1);
  bb->type = htole32(2);
}

static struct inode *find_inode(uint8_t *map, int32_t nb1, int32_t nbi, const char *name)
{
  int32_t base = 1 + nb1;
  struct inode *researched = NULL;
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = (struct inode *)(map + (uint64_t)(base + i) * 4096);
    if (le32toh(in->flags) & 1)
    {
      if (strcmp(in->filename, name) == 0)
        return in;
    }
  }
  return researched;
}

static uint64_t alloc_data_block(uint8_t *map)
{
  struct pignoufs *sb = (struct pignoufs *)map;
  int32_t nb1, nbi, nba, nbb, nbl;
  nbi = le32toh(sb->nb_i);
  nba = le32toh(sb->nb_a);
  nbb = le32toh(sb->nb_b);
  nbl = le32toh(sb->nb_l);
  nb1 = nbb - 1 - nbi - nba;
  for (uint64_t bloc_adresse_libre = 1 + nb1 + nbi; bloc_adresse_libre < nbb; bloc_adresse_libre++)
  {
    uint64_t idx = bloc_adresse_libre / 32000;
    uint64_t bit = bloc_adresse_libre % 32000;
    uint8_t *map2 = map + (uint64_t)(idx+1) * 4096;
    struct bitmap_block *bb = (struct bitmap_block *)map2;
    if ((bb->bits[bit / 8] & (1 << (bit % 8))) != 0)
    {
      bitmap_alloc(map, bloc_adresse_libre);
      decremente_lbl(map);
      return bloc_adresse_libre;
    }
  }
  fprintf(stderr, "Erreur: pas de blocs de données libres disponibles\n");
  return -1;
}

static void bitmap_dealloc(uint8_t *map, int32_t blknum)
{
  int32_t idx = blknum / 32000;
  int32_t bit = blknum % 32000;
  uint8_t *data_position = map + (uint64_t)(1 + idx) * 4096;
  struct bitmap_block *bb = (struct bitmap_block *)data_position;
  bb->bits[bit / 8] |= (1 << (bit % 8));
  calcul_sha1(bb->bits, 4000, bb->sha1);
  bb->type = htole32(2);
}

static void dealloc_data_block(struct inode *in, uint8_t *map)
{
  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  for(int i = 0; i < 900; i++)
  {
    if(in->direct_blocks[i] != -1){
      bitmap_dealloc(map, in->direct_blocks[i]);
      in->direct_blocks[i] = -1;
      decremente_lbl(map);
    }
  }

  if(in->double_indirect_block != -1){
    struct address_block *dbl = (struct address_block *)(map + (uint64_t)le32toh(in->double_indirect_block) * 4096);
    if(le32toh(dbl->type) == 6){
      for(int i = 0; i < 1000; i++){
        if(dbl->addresses[i] != -1){
          bitmap_dealloc(map, dbl->addresses[i]);
          dbl->addresses[i] = -1;
          decremente_lbl(map);
        }
      }
    }else{
      for(int i = 0; i < 1000; i++){
        struct address_block *sib = (struct address_block *)(map + (uint64_t)le32toh(dbl->addresses[i]) * 4096);
        for(int j = 0; j < 1000; j++){
          if(sib->addresses[j] != -1){
            bitmap_dealloc(map, sib->addresses[j]);
            sib->addresses[j] = -1;
            decremente_lbl(map);
          }
        }
        bitmap_dealloc(map, dbl->addresses[i]);
        dbl->addresses[i] = -1;
        decremente_lbl(map);
      }
    }
    bitmap_dealloc(map, in->double_indirect_block);
    in->double_indirect_block = -1;
    decremente_lbl(map);
  }
  in->file_size = 0;
  in->modification_time = htole32(time(NULL));
  calcul_sha1(in, 4000, in->sha1);
}

static int create_file(uint8_t *map, const char *filename)
{
  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct pignoufs *sb = (struct pignoufs *)map;
  int32_t base = 1 + nb1;
  int32_t free_idx = -1;

  // Scan inodes
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = (struct inode *)(map + (uint64_t)(base + i) * 4096);
    if (in->flags & 1)
    {
      if (strcmp(in->filename, filename) == 0)
      {
        in->modification_time = time(NULL);
        calcul_sha1(in, 4000, in->sha1);
        return 0;
      }
    }
    else if (free_idx < 0)
    {
      free_idx = i;
    }
  }
  if (free_idx < 0)
  {
    fprintf(stderr, "touch: aucun inode libre disponible\n");
    return -1;
  }
  // Initialiser l'inode et l'allouer dans le bitmap
  struct inode *in = (struct inode *)(map + (uint64_t)(base + free_idx) * 4096);
  init_inode(in, filename, false);
  int32_t inode_blk = base + free_idx;
  printf("create file\n");
  bitmap_alloc(map, inode_blk);

  // Mettre à jour les compteurs du superbloc et son SHA1

  sb->nb_f += 1;
  calcul_sha1(map, 4000, map + 4000);

  return 0;
}

// On récupère le dernier bloc écrit
static int32_t get_last_data_block(uint8_t *map, struct inode *in, int32_t nb1, int32_t nbi, int32_t nba, int32_t nbb) {
  uint32_t size = le32toh(in->file_size);  
  int32_t last_index = (size - 1) / 4000;
  // printf("last_index = %d\n", last_index);
  if(le32toh(in->double_indirect_block) == -1){
    return in->direct_blocks[last_index];
  }else{
    last_index -= 900;
    struct address_block *dbl = (struct address_block *)(map + (uint64_t)le32toh(in->double_indirect_block) * 4096);
    if(le32toh(dbl->type) == 6){
      return dbl->addresses[last_index];
    }else{
      int outer = last_index / 1000;
      int inner = last_index % 1000;
      struct address_block *sib = (struct address_block *)(map + (uint64_t)le32toh(dbl->addresses[outer]) * 4096);
      return sib->addresses[inner];
    }
  }
}

// Pour les calculs ont suppose qu'on ait des blocs pleins
static uint32_t get_last_data_block_null(uint8_t *map, struct inode *in, int32_t nb1, int32_t nbi, int32_t nba, int32_t nbb) {
  uint32_t size = in->file_size;
  uint32_t last_index = size / 4000;
  if(last_index == 900 + (1000*1000)){
    perror("Erreur: limite de 1000900 blocs de données atteinte");
    return -2;
  }
  if(in->double_indirect_block == -1){
    if(last_index >= 900){
      uint64_t dbl_blk = alloc_data_block(map);
      in->double_indirect_block = dbl_blk;
      struct address_block *dbl = (struct address_block *)(map + (uint64_t)dbl_blk * 4096);
      memset(dbl->addresses, -1, sizeof(dbl->addresses));
      calcul_sha1(dbl->addresses, 4000, dbl->sha1);
      dbl->type = 6;
      goto rec1;
    }
    uint64_t dbl_blk = alloc_data_block(map);
    in->direct_blocks[last_index] = dbl_blk;  
    return in->direct_blocks[last_index];
  }else{
    rec1:
    last_index -= 900;
    struct address_block *dbl = (struct address_block *)(map + (uint64_t)in->double_indirect_block * 4096);
    if(le32toh(dbl->type) == 6){
      if(last_index >= 1000){
        uint64_t save = in->double_indirect_block;
        uint64_t dbl_blk = alloc_data_block(map);
        in->double_indirect_block = dbl_blk;
        struct address_block *dbl2 = (struct address_block *)(map + (uint64_t)dbl_blk * 4096);
        memset(dbl2->addresses, -1, sizeof(dbl2->addresses));
        calcul_sha1(dbl2->addresses, 4000, dbl2->sha1);
        dbl2->type = 7;
        dbl2->addresses[0] = save;
        dbl = dbl2;
        goto rec2;
      }
      if(dbl->addresses[last_index] == -1){
        uint64_t dbl_blk = alloc_data_block(map);
        dbl->addresses[last_index] = dbl_blk;
      }
      return dbl->addresses[last_index];
    }else{
      rec2:
      int outer = last_index / 1000;
      int inner = last_index % 1000;
      if(dbl->addresses[outer] == -1){
        uint64_t dbl_blk = alloc_data_block(map);
        dbl->addresses[outer] = dbl_blk;
        struct address_block *dbl2 = (struct address_block *)(map + (uint64_t)dbl_blk * 4096);
        memset(dbl2->addresses, -1, sizeof(dbl2->addresses));
        calcul_sha1(dbl2->addresses, 4000, dbl2->sha1);
        dbl2->type = 6;
      }

      struct address_block *sib = (struct address_block *)(map + (uint64_t)dbl->addresses[outer] * 4096);
      
      if(sib->addresses[inner] == -1){
        uint64_t dbl_blk = alloc_data_block(map);
        sib->addresses[inner] = dbl_blk;
      }
      return sib->addresses[inner];
    }
  }
}


#endif