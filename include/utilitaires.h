#ifndef UTILITAIRES_H
#define UTILITAIRES_H

#include "structures.h"

// Calcul SHA1 d'un bloc de données
static void calcul_sha1(const void *data, size_t len, uint8_t *out)
{
  EVP_MD_CTX *ctx = EVP_MD_CTX_new(); // Crée un contexte pour le calcul
  if (!ctx) {
    perror("Erreur : Impossible de créer le contexte EVP");
    return;
  }
  if (EVP_DigestInit_ex(ctx, EVP_sha1(), NULL) != 1) {
    perror("Erreur : EVP_DigestInit_ex a échoué");
    EVP_MD_CTX_free(ctx);
    return;
  }
  if (EVP_DigestUpdate(ctx, data, len) != 1) {
    perror("Erreur : EVP_DigestUpdate a échoué");
    EVP_MD_CTX_free(ctx);
    return;
  }
  if (EVP_DigestFinal_ex(ctx, out, NULL) != 1) {
    perror("Erreur : EVP_DigestFinal_ex a échoué");
    EVP_MD_CTX_free(ctx);
    return;
  }
  EVP_MD_CTX_free(ctx);
}

// Récupération des données du conteneur
static void get_conteneur_data(uint8_t *map, int32_t *nb1, int32_t *nbi, int32_t *nba, int32_t *nbb)
{
  struct pignoufs *sb = (struct pignoufs *)map;
  *nbi = le32toh(sb->nb_i);
  *nba = le32toh(sb->nb_a);
  *nbb = le32toh(sb->nb_b);
  *nb1 = *nbb - 1 - *nbi - *nba;
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

static bool check_bitmap_bit(uint8_t *map, int32_t blknum)
{
  int32_t idx = blknum / 32000;
  int32_t bit = blknum % 32000;
  uint8_t *data_position = map + (int64_t)(1 + idx) * 4096;
  struct bitmap_block *bb = (struct bitmap_block *)data_position;
  return (bb->bits[bit / 8] & (1 << (bit % 8))) == 0;
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
  uint8_t *data_position = map + (int64_t)(1 + idx) * 4096;
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
    struct inode *in = (struct inode *)(map + (int64_t)(base + i) * 4096);
    if (le32toh(in->flags) & 1)
    {
      if (strcmp(in->filename, name) == 0)
        return in;
    }
  }
  return researched;
}

static int32_t find_node_pos(uint8_t *map, const char *name)
{
  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  int32_t base = 1 + nb1;
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = (struct inode *)(map + (int64_t)(base + i) * 4096);
    if (le32toh(in->flags) & 1)
    {
      if (strcmp(in->filename, name) == 0)
        return base + i;
    }
  }
  return -1;
}

static int32_t alloc_data_block(uint8_t *map)
{
  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  for (int32_t bloc_adresse_libre = 1 + nb1 + nbi; bloc_adresse_libre < nbb; bloc_adresse_libre++)
  {
    int32_t idx = bloc_adresse_libre / 32000;
    int32_t bit = bloc_adresse_libre % 32000;
    uint8_t *map2 = map + (int64_t)(idx+1) * 4096;
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
  blknum = le32toh(blknum);
  int32_t idx = blknum / 32000;
  int32_t bit = blknum % 32000;
  uint8_t *data_position = map + (int64_t)(1 + idx) * 4096;
  struct bitmap_block *bb = (struct bitmap_block *)data_position;
  bb->bits[bit / 8] |= (1 << (bit % 8));
  calcul_sha1(bb->bits, 4000, bb->sha1);
}

static void dealloc_data_block(struct inode *in, uint8_t *map)
{
  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  for(int i = 0; i < 900; i++)
  {
    if((int32_t)le32toh(in->direct_blocks[i]) <0){
      struct data_block *db = (struct data_block *)(map + (int64_t)le32toh(in->direct_blocks[i]) * 4096);
      db->type = htole32(4);
      calcul_sha1(db->data, 4000, db->sha1);
      bitmap_dealloc(map, in->direct_blocks[i]);
      in->direct_blocks[i] = -1;
      incremente_lbl(map);
    }
  }
  if((int32_t)le32toh(in->double_indirect_block) <0){
    struct address_block *dbl = (struct address_block *)(map + (int64_t)le32toh(in->double_indirect_block) * 4096);
    if((int32_t)le32toh(dbl->type) == 6){
      for(int i = 0; i < 1000; i++){
        if(dbl->addresses[i] <0){
          struct data_block *db = (struct data_block *)(map + (int64_t)le32toh(dbl->addresses[i]) * 4096);
          db->type = htole32(4);
          calcul_sha1(db->data, 4000, db->sha1);
          bitmap_dealloc(map, dbl->addresses[i]);
          dbl->addresses[i] = htole32(-1);
          incremente_lbl(map);
        }
      }
    }else{
      for(int i = 0; i < 1000; i++){
        struct address_block *sib = (struct address_block *)(map + (int64_t)le32toh(dbl->addresses[i]) * 4096);
        // printf("%d and %u %u\n", i, (int32_t)le32toh(dbl->addresses[i]), (int32_t)le32toh(in->double_indirect_block));
        for(int j = 0; j < 1000; j++){
          // printf("dealloc indirect block here\n");
          // printf("i j %d %d\n", i, j);
          // printf("address %u\n", (uint32_t)le32toh(in->double_indirect_block));
          // printf("address %u\n", (uint32_t)le32toh(dbl->addresses[i]));
          // printf("address %u\n", (uint32_t)le32toh(sib->addresses[j]));
          if((int32_t)le32toh(sib->addresses[j]) <0){
            struct data_block *db = (struct data_block *)(map + (int64_t)le32toh(sib->addresses[j]) * 4096);
            
            db->type = htole32(4);

            calcul_sha1(db->data, 4000, db->sha1);
            bitmap_dealloc(map, sib->addresses[j]);
            sib->addresses[j] = htole32(-1);
            incremente_lbl(map);
          }
        }
        // printf("dealloc indirect block\n");
        struct address_block *dbl2 = (struct address_block *)(map + (int64_t)le32toh(dbl->addresses[i]) * 4096);
        dbl2->type = htole32(4);
        calcul_sha1(dbl2->addresses, 4000, dbl2->sha1);
        bitmap_dealloc(map, dbl->addresses[i]);
        dbl->addresses[i] = htole32(-1);
        incremente_lbl(map);
      }
      printf("dealloc double indirect block\n");
    }
    bitmap_dealloc(map, in->double_indirect_block);
    in->double_indirect_block = htole32(-1);
    incremente_lbl(map);
  }
  in->file_size = htole32(0);
  in->modification_time = htole32(time(NULL));
  calcul_sha1(in, 4000, in->sha1);
}

static void delete_inode(struct inode *in, uint8_t *map)
{
  dealloc_data_block(in, map);
  int32_t pos = find_node_pos(map, in->filename);
  if (pos == -1)
  {
    fprintf(stderr, "Erreur: inode introuvable\n");
    return;
  }
  in->flags = htole32(0);
  in->file_size = htole32(0);
  in->creation_time = htole32(0);
  in->access_time = htole32(0);
  in->modification_time = htole32(0);
  memset(in->filename, 0, sizeof(in->filename));
  memset(in->extensions, 0, sizeof in->extensions);
  bitmap_dealloc(map, pos);
  struct pignoufs *sb = (struct pignoufs *)map;
  sb->nb_f--;
  calcul_sha1(in, 4000, in->sha1);
  in->type = htole32(3);
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
    struct inode *in = (struct inode *)(map + (int64_t)(base + i) * 4096);
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
  struct inode *in = (struct inode *)(map + (int64_t)(base + free_idx) * 4096);
  init_inode(in, filename, false);
  int32_t inode_blk = base + free_idx;
  printf("create file\n");
  bitmap_alloc(map, inode_blk);

  // Mettre à jour les compteurs du superbloc et son SHA1

  sb->nb_f++;
  calcul_sha1(map, 4000, map + 4000);

  return 0;
}

// On récupère le dernier bloc écrit
static int32_t get_last_data_block(uint8_t *map, struct inode *in) {
  uint32_t size = le32toh(in->file_size);  
  int32_t last_index = (size - 1) / 4000;
  if((int32_t)le32toh(in->double_indirect_block) == -1){
    return (int32_t)le32toh(in->direct_blocks[last_index]);
  }else{
    last_index -= 900;
    struct address_block *dbl = (struct address_block *)(map + (int64_t)le32toh(in->double_indirect_block) * 4096);
    if(le32toh(dbl->type) == 6){
      return le32toh(dbl->addresses[last_index]);
    }else{
      int outer = last_index / 1000;
      int inner = last_index % 1000;
      struct address_block *sib = (struct address_block *)(map + (int64_t)le32toh(dbl->addresses[outer]) * 4096);
      return le32toh(sib->addresses[inner]);
    }
  }
}

// Pour les calculs ont suppose qu'on ait des blocs pleins
static int32_t get_last_data_block_null(uint8_t *map, struct inode *in) {
  uint32_t size = le32toh(in->file_size);
  int32_t last_index = size / 4000;
  if(last_index == 900 + (1000*1000)){
    perror("Erreur: limite de 1000900 blocs de données atteinte");
    return -2;
  }
  if(in->double_indirect_block == -1){
    if(last_index >= 900){
      int32_t dbl_blk = alloc_data_block(map);
      in->double_indirect_block = htole32(dbl_blk);
      struct address_block *dbl = (struct address_block *)(map + (int64_t)dbl_blk * 4096);
      memset(dbl->addresses, -1, sizeof(dbl->addresses));
      calcul_sha1(dbl->addresses, 4000, dbl->sha1);
      dbl->type = htole32(6);
      goto rec1;
    }
    int32_t dbl_blk = alloc_data_block(map);
    in->direct_blocks[last_index] = htole32(dbl_blk);  
    return dbl_blk;
  }else{
    rec1:
    last_index -= 900;
    struct address_block *dbl = (struct address_block *)(map + (int64_t)in->double_indirect_block * 4096);
    if(le32toh(dbl->type) == 6){
      if(last_index >= 1000){
        int32_t save = in->double_indirect_block; // Pas besoin de conversion, on garde juste la valeur telle quelle
        int32_t dbl_blk = alloc_data_block(map);
        in->double_indirect_block = htole32(dbl_blk);
        struct address_block *dbl2 = (struct address_block *)(map + (int64_t)dbl_blk * 4096);
        memset(dbl2->addresses, -1, sizeof(dbl2->addresses));
        dbl2->type = htole32(7);
        dbl2->addresses[0] = save;
        calcul_sha1(dbl2->addresses, 4000, dbl2->sha1);
        dbl = dbl2;
        goto rec2;
      }
      if((int32_t)le32toh(dbl->addresses[last_index]) == -1){
        int32_t dbl_blk = alloc_data_block(map);
        dbl->addresses[last_index] = htole32(dbl_blk);
      }
      return le32toh(dbl->addresses[last_index]);
    }else{
      rec2:
      int outer = last_index / 1000;
      int inner = last_index % 1000;
      if((int32_t)le32toh(dbl->addresses[outer]) == -1){
        int32_t dbl_blk = alloc_data_block(map);
        dbl->addresses[outer] = htole32(dbl_blk);
        struct address_block *dbl2 = (struct address_block *)(map + (int64_t)dbl_blk * 4096);
        memset(dbl2->addresses, -1, sizeof(dbl2->addresses));
        calcul_sha1(dbl2->addresses, 4000, dbl2->sha1);
        dbl2->type = htole32(6);
      }

      struct address_block *sib = (struct address_block *)(map + (int64_t)le32toh(dbl->addresses[outer]) * 4096);
      
      if((int32_t)le32toh(sib->addresses[inner]) == -1){
        int32_t dbl_blk = alloc_data_block(map);
        sib->addresses[inner] = htole32(dbl_blk);
      }
      return le32toh(sib->addresses[inner]);
    }
  }
}

void afficher_bloc(const uint8_t *bloc, size_t taille) {
  for (size_t i = 0; i < taille; i++) {
      printf("%02x ", bloc[i]);
      if ((i + 1) % 16 == 0) { // Affiche 16 octets par ligne
          printf("\n");
      }
  }
  printf("\n");
}


#endif