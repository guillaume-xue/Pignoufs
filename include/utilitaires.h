#ifndef UTILITAIRES_H
#define UTILITAIRES_H

#include "structures.h"
#include "sha1.h"

// Gestion centralisée des erreurs
static int print_error(const char *msg)
{
  fprintf(stderr, "%s\n", msg);
  return 1;
}
static void fatal_error(const char *msg)
{
  fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}


// Helpers pour accéder aux blocs
static struct pignoufs *get_superblock(uint8_t *map)
{
  struct pignoufs *sb = (struct pignoufs *)map;
  check_sha1(sb, 4000, sb->sha1);
  return sb;
}
static struct bitmap_block *get_bitmap_block(uint8_t *map, int32_t b)
{
  struct bitmap_block *bb = (struct bitmap_block *)(map + (int64_t)b * 4096);
  check_sha1(bb->bits, 4000, bb->sha1);
  return bb;
}
static struct inode *get_inode(uint8_t *map, int32_t base, int i)
{
  struct inode *in = (struct inode *)(map + (int64_t)(base + i) * 4096);
  check_sha1(in, 4000, in->sha1);
  return in;
}
static struct data_block *get_data_block(uint8_t *map, int32_t b)
{
  struct data_block *db = (struct data_block *)(map + (int64_t)b * 4096);
  check_sha1(db->data, 4000, db->sha1);
  return db;
}
static struct address_block *get_address_block(uint8_t *map, int32_t b)
{
  struct address_block *ab = (struct address_block *)(map + (int64_t)b * 4096);
  check_sha1(ab->addresses, 4000, ab->sha1);
  return ab;
}

// Récupération des données du conteneur
static void get_conteneur_data(uint8_t *map, int32_t *nb1, int32_t *nbi, int32_t *nba, int32_t *nbb)
{
  struct pignoufs *sb = get_superblock(map);
  *nbi = FROM_LE32(sb->nb_i);
  *nba = FROM_LE32(sb->nb_a);
  *nbb = FROM_LE32(sb->nb_b);
  *nb1 = *nbb - 1 - *nbi - *nba;
}

static void decremente_lbl(uint8_t *map)
{
  struct pignoufs *sb = get_superblock(map);
  sb->nb_l--;
  calcul_sha1(map, 4000, map + 4000);
}

static void incremente_lbl(uint8_t *map)
{
  struct pignoufs *sb = get_superblock(map);
  sb->nb_l++;
  calcul_sha1(map, 4000, map + 4000);
}

static void decrement_nb_f(uint8_t *map)
{
  struct pignoufs *sb = get_superblock(map);
  sb->nb_f--;
  calcul_sha1(map, 4000, map + 4000);
}

static void increment_nb_f(uint8_t *map)
{
  struct pignoufs *sb = get_superblock(map);
  sb->nb_f++;
  calcul_sha1(map, 4000, map + 4000);
}

static bool check_bitmap_bit(uint8_t *map, int32_t blknum)
{
  int32_t idx = blknum / 32000;
  int32_t bit = blknum % 32000;
  struct bitmap_block *bb = get_bitmap_block(map, idx + 1);
  return (bb->bits[bit / 8] & (1 << (bit % 8))) == 0;
}

// Initialisation d'un inode, fichier simple pour l'instant
static void init_inode(struct inode *in, const char *name, bool type)
{
  time_t now = time(NULL);
  in->flags = TO_LE32((1 << 0)        /* existence */
                      | (1 << 1)      /* permission de lecture */
                      | (1 << 2)      /* permission d'écriture */
                      | (0 << 3)      /* verrouillage en lecture */
                      | (0 << 4)      /* verrouillage en écriture */
                      | (type << 5)); /* 0 fichier, 1 répertoire */
  in->file_size = TO_LE32(0);
  in->creation_time = TO_LE32(now);
  in->access_time = TO_LE32(now);
  in->modification_time = TO_LE32(now);
  strncpy(in->filename, name, sizeof(in->filename) - 1);
  in->filename[sizeof(in->filename) - 1] = '\0';
  for (int i = 0; i < 900; i++)
  {
    in->direct_blocks[i] = TO_LE32(-1);
  }
  in->double_indirect_block = TO_LE32(-1);
  memset(in->extensions, 0, sizeof in->extensions);
  calcul_sha1(in, 4000, in->sha1);
  in->type = TO_LE32(3);
}

// Ouverture du système de fichiers et projection en mémoire
static int open_fs(const char *fsname, uint8_t **mapp, size_t *sizep)
{
  int fd = open(fsname, O_RDWR);
  if (fd < 0)
  {
    fatal_error("open: erreur d'ouverture du fichier");
  }
  struct stat st;
  if (fstat(fd, &st) < 0)
  {
    print_error("fstat: erreur");
    close(fd);
    return -1;
  }
  *sizep = st.st_size;
  *mapp = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (*mapp == MAP_FAILED)
  {
    print_error("mmap: erreur");
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
  struct bitmap_block *bb = get_bitmap_block(map, idx + 1);
  bb->bits[bit / 8] &= ~(1 << (bit % 8));
  calcul_sha1(bb->bits, 4000, bb->sha1);
  bb->type = TO_LE32(2);
}

static struct inode *find_inode(uint8_t *map, int32_t nb1, int32_t nbi, const char *name)
{
  struct inode *researched = NULL;
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = get_inode(map, 1 + nb1, i);
    if (FROM_LE32(in->flags) & 1)
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
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = get_inode(map, 1 + nb1, i);
    if (FROM_LE32(in->flags) & 1)
    {
      if (strcmp(in->filename, name) == 0)
        return 1 + nb1 + i;
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
    struct bitmap_block *bb = get_bitmap_block(map, idx + 1);
    if ((bb->bits[bit / 8] & (1 << (bit % 8))) != 0)
    {
      bitmap_alloc(map, bloc_adresse_libre);
      decremente_lbl(map);
      return bloc_adresse_libre;
    }
  }
  print_error("Erreur: pas de blocs de données libres disponibles");
  return -1;
}

static void bitmap_dealloc(uint8_t *map, int32_t blknum)
{
  blknum = FROM_LE32(blknum);
  int32_t idx = blknum / 32000;
  int32_t bit = blknum % 32000;
  struct bitmap_block *bb = get_bitmap_block(map, idx + 1);
  bb->bits[bit / 8] |= (1 << (bit % 8));
  calcul_sha1(bb->bits, 4000, bb->sha1);
}

static void dealloc_data_block(struct inode *in, uint8_t *map)
{
  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  for (int i = 0; i < 900; i++)
  {
    if ((int32_t)FROM_LE32(in->direct_blocks[i]) >= 0)
    {
      struct data_block *db = get_data_block(map, in->direct_blocks[i]);
      db->type = TO_LE32(4);
      calcul_sha1(db->data, 4000, db->sha1);
      bitmap_dealloc(map, in->direct_blocks[i]);
      in->direct_blocks[i] = TO_LE32(-1);
      incremente_lbl(map);
    }
  }
  if ((int32_t)FROM_LE32(in->double_indirect_block) >= 0)
  {
    struct address_block *dbl = get_address_block(map, FROM_LE32(in->double_indirect_block));
    if ((int32_t)FROM_LE32(dbl->type) == 6)
    {
      for (int i = 0; i < 1000; i++)
      {
        if (dbl->addresses[i] < 0)
          break;
        struct data_block *db = get_data_block(map, dbl->addresses[i]);
        db->type = TO_LE32(4);
        calcul_sha1(db->data, 4000, db->sha1);
        bitmap_dealloc(map, dbl->addresses[i]);
        dbl->addresses[i] = TO_LE32(-1);
        incremente_lbl(map);
      }
    }
    else
    {
      for (int i = 0; i < 1000; i++)
      {
        if ((int32_t)FROM_LE32(dbl->addresses[i]) < 0)
          break;
        struct address_block *sib = get_address_block(map, dbl->addresses[i]);
        for (int j = 0; j < 1000; j++)
        {
          if ((int32_t)FROM_LE32(sib->addresses[j]) < 0)
            break;
          struct data_block *db = get_data_block(map, sib->addresses[j]);
          db->type = TO_LE32(4);
          calcul_sha1(db->data, 4000, db->sha1);
          bitmap_dealloc(map, sib->addresses[j]);
          sib->addresses[j] = TO_LE32(-1);
          incremente_lbl(map);
        }
        sib->type = TO_LE32(4);
        calcul_sha1(sib->addresses, 4000, sib->sha1);
        bitmap_dealloc(map, dbl->addresses[i]);
        dbl->addresses[i] = TO_LE32(-1);
        incremente_lbl(map);
      }
    }
    calcul_sha1(dbl->addresses, 4000, dbl->sha1);
    dbl->type = TO_LE32(4);
    bitmap_dealloc(map, in->double_indirect_block);
    incremente_lbl(map);
  }
  in->double_indirect_block = TO_LE32(-1);
  in->file_size = TO_LE32(0);
  in->modification_time = TO_LE32(time(NULL));
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
  in->flags = TO_LE32(0);
  in->file_size = TO_LE32(0);
  in->creation_time = TO_LE32(0);
  in->access_time = TO_LE32(0);
  in->modification_time = TO_LE32(0);
  memset(in->filename, 0, sizeof(in->filename));
  memset(in->extensions, 0, sizeof in->extensions);
  bitmap_dealloc(map, pos);
  decrement_nb_f(map);
  calcul_sha1(in, 4000, in->sha1);
  in->type = TO_LE32(3);
}

static int create_file(uint8_t *map, const char *filename)
{
  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  int32_t free_idx = -1;

  // Scan inodes
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = get_inode(map, 1 + nb1, i);
    if (FROM_LE32(in->flags) & 1)
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
    return print_error("touch: aucun inode libre disponible");
  }
  // Initialiser l'inode et l'allouer dans le bitmap
  struct inode *in = get_inode(map, 1 + nb1, free_idx);
  init_inode(in, filename, false);
  int32_t inode_blk = 1 + nb1 + free_idx;
  bitmap_alloc(map, inode_blk);
  // Mettre à jour les compteurs du superbloc et son SHA1
  increment_nb_f(map);
  return 0;
}

static int create_directory_racine(uint8_t *map, const char *dirname)
{
  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  int32_t free_idx = -1;

  // Scan inodes
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = get_inode(map, 1 + nb1, i);
    if (FROM_LE32(in->flags) & 1)
    {
      if (strcmp(in->filename, dirname) == 0)
      {
        // Déjà existant, on met à jour la date de modification
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
    return print_error("mkdir: aucun inode libre disponible");
  }
  // Initialiser l'inode comme répertoire et l'allouer dans le bitmap
  struct inode *in = get_inode(map, 1 + nb1, free_idx);
  init_inode(in, dirname, true); // true = répertoire
  in->padding[0] = 1;            // Racine
  int32_t inode_blk = 1 + nb1 + free_idx;
  bitmap_alloc(map, inode_blk);

  increment_nb_f(map);
  return 0;
}

void delete_children(struct inode *dir, uint8_t *map)
{
  for (int i = 0; i < 900; i++)
  {
    int32_t child_idx = FROM_LE32(dir->direct_blocks[i]);
    if (child_idx == -1)
      continue;
    struct inode *child = get_inode(map, 1 + nb1, child_idx);
    if ((FROM_LE32(child->flags) & (1 << 5)))
    {
      // Si c'est un sous-répertoire, suppression récursive
      delete_children(child, map);
    }
    delete_inode(child, map);
    dir->direct_blocks[i] = TO_LE32(-1);
  }
  calcul_sha1(dir, 4000, dir->sha1);
}

// On récupère le dernier bloc écrit
static int32_t get_last_data_block(uint8_t *map, struct inode *in)
{
  uint32_t size = FROM_LE32(in->file_size);
  int32_t last_index = (size - 1) / 4000;
  if ((int32_t)FROM_LE32(in->double_indirect_block) == -1)
  {
    return (int32_t)FROM_LE32(in->direct_blocks[last_index]);
  }
  else
  {
    last_index -= 900;
    struct address_block *dbl = get_address_block(map, FROM_LE32(in->double_indirect_block));
    if (FROM_LE32(dbl->type) == 6)
    {
      return FROM_LE32(dbl->addresses[last_index]);
    }
    else
    {
      int outer = last_index / 1000;
      int inner = last_index % 1000;
      struct address_block *sib = get_address_block(map, FROM_LE32(dbl->addresses[outer]));
      return FROM_LE32(sib->addresses[inner]);
    }
  }
}

// Pour les calculs ont suppose qu'on ait des blocs pleins
static int32_t get_last_data_block_null(uint8_t *map, struct inode *in)
{
  uint32_t size = FROM_LE32(in->file_size);
  int32_t last_index = size / 4000;
  if (last_index == 900 + (1000 * 1000))
  {
    perror("Erreur: limite de 1000900 blocs de données atteinte");
    return -2;
  }
  if (in->double_indirect_block == -1)
  {
    if (last_index >= 900)
    {
      int32_t dbl_blk = alloc_data_block(map);
      in->double_indirect_block = TO_LE32(dbl_blk);
      struct address_block *dbl = get_address_block(map, dbl_blk);
      memset(dbl->addresses, -1, sizeof(dbl->addresses));
      calcul_sha1(dbl->addresses, 4000, dbl->sha1);
      dbl->type = TO_LE32(6);
      goto rec1;
    }
    int32_t dbl_blk = alloc_data_block(map);
    in->direct_blocks[last_index] = TO_LE32(dbl_blk);
    return dbl_blk;
  }
  else
  {
  rec1:
    last_index -= 900;
    struct address_block *dbl = get_address_block(map, FROM_LE32(in->double_indirect_block));
    if (FROM_LE32(dbl->type) == 6)
    {
      if (last_index >= 1000)
      {
        int32_t save = in->double_indirect_block; // Pas besoin de conversion, on garde juste la valeur telle quelle
        int32_t dbl_blk = alloc_data_block(map);
        in->double_indirect_block = TO_LE32(dbl_blk);
        struct address_block *dbl2 = get_address_block(map, dbl_blk);
        memset(dbl2->addresses, -1, sizeof(dbl2->addresses));
        dbl2->type = TO_LE32(7);
        dbl2->addresses[0] = save;
        calcul_sha1(dbl2->addresses, 4000, dbl2->sha1);
        dbl = dbl2;
        goto rec2;
      }
      if ((int32_t)FROM_LE32(dbl->addresses[last_index]) == -1)
      {
        int32_t dbl_blk = alloc_data_block(map);
        dbl->addresses[last_index] = TO_LE32(dbl_blk);
      }
      return FROM_LE32(dbl->addresses[last_index]);
    }
    else
    {
    rec2:
      int outer = last_index / 1000;
      int inner = last_index % 1000;
      if ((int32_t)FROM_LE32(dbl->addresses[outer]) == -1)
      {
        int32_t dbl_blk = alloc_data_block(map);
        dbl->addresses[outer] = TO_LE32(dbl_blk);
        struct address_block *dbl2 = get_address_block(map, dbl_blk);
        memset(dbl2->addresses, -1, sizeof(dbl2->addresses));
        calcul_sha1(dbl2->addresses, 4000, dbl2->sha1);
        dbl2->type = TO_LE32(6);
      }

      struct address_block *sib = get_address_block(map, FROM_LE32(dbl->addresses[outer]));
      if ((int32_t)FROM_LE32(sib->addresses[inner]) == -1)
      {
        int32_t dbl_blk = alloc_data_block(map);
        sib->addresses[inner] = TO_LE32(dbl_blk);
      }
      return FROM_LE32(sib->addresses[inner]);
    }
  }
}

// Verrouiller un bloc pour lecture ou écriture
static int lock_block(int fd, int64_t block_offset, int lock_type)
{
  struct flock fl = {0};
  fl.l_type = lock_type; // F_RDLCK pour lecture, F_WRLCK pour écriture
  fl.l_whence = SEEK_SET;
  fl.l_start = block_offset;
  fl.l_len = 4096; // Taille d'un bloc
  return fcntl(fd, F_SETLKW, &fl);
}

// Déverrouiller un bloc
static int unlock_block(int fd, int64_t block_offset)
{
  struct flock fl = {0};
  fl.l_type = F_UNLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = block_offset;
  fl.l_len = 4096; // Taille d'un bloc
  return fcntl(fd, F_SETLK, &fl);
}

static void split_path(const char *path, char *parent_path, char *dir_name)
{
  const char *last_slash = strrchr(path, '/');
  if (last_slash)
  {
    size_t parent_len = last_slash - path;
    strncpy(parent_path, path, parent_len);
    parent_path[parent_len] = '\0';
    strncpy(dir_name, last_slash + 1, 255);
    dir_name[255] = '\0';
  }
  else
  {
    // Pas de slash dans le chemin, le répertoire parent est vide
    parent_path[0] = '\0';
    strncpy(dir_name, path, 255);
    dir_name[255] = '\0';
  }
}

#endif