#include "../include/structures.h"

// Calcul SHA1 d'un bloc de données
static void calcul_sha1(const void *data, size_t len, uint8_t *out)
{
  SHA_CTX ctx;
  SHA1_Init(&ctx);
  SHA1_Update(&ctx, data, len);
  SHA1_Final(out, &ctx);
}

// Récupération des données du conteneur
static void get_conteneur_data(uint8_t *map, int32_t *nb1, int32_t *nbi, int32_t *nba)
{
  struct pignoufs *sb = (struct pignoufs *)map;
  *nbi = le32toh(sb->nb_i);
  *nba = le32toh(sb->nb_a);
  *nb1 = le32toh(sb->nb_b) - 1 - *nbi - *nba;
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
  uint8_t *blk = map + (1 + idx) * 4096;
  struct bitmap_block *bb = (struct bitmap_block *)blk;
  bb->bits[bit / 8] &= ~(1 << (bit % 8));
  calcul_sha1(bb->bits, 4000, bb->sha1);
  bb->type = htole32(2); // Convert to little endian
}

// Fonction de création du système de fichiers (mkfs)
void cmd_mkfs(int argc, char *argv[])
{
  const char *fsname = argv[1];
  if (argc < 4)
  {
    fprintf(stderr, "Usage: mkfs <fsname> <nbi> <nba>\n");
    return;
  }
  char *endptr;
  int32_t nbi = strtol(argv[2], &endptr, 10);
  if (*endptr != '\0')
  {
    fprintf(stderr, "Erreur: %s n'est pas un nombre valide.\n", argv[2]);
    return;
  }

  int32_t nba = strtol(argv[3], &endptr, 10);
  if (*endptr != '\0')
  {
    fprintf(stderr, "Erreur: %s n'est pas un nombre valide.\n", argv[3]);
    return;
  }

  // Calcul initial du nombre de blocs
  int32_t nb1 = (int32_t)ceil((1 + nbi + nba) / 32000.0);
  int32_t nbb = 1 + nb1 + nbi + nba;
  int32_t nb1_new;
  // Boucle pour recalculer jusqu'à convergence
  while (1)
  {
    nb1_new = (int32_t)ceil(nbb / 32000.0);
    if (nb1_new == nb1)
    {
      break;
    }
    nb1 = nb1_new;
    nbb = 1 + nb1 + nbi + nba;
  }
  // Création et dimensionnement du conteneur
  int fd = open(fsname, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
  {
    perror("open");
    return EXIT_FAILURE;
  }
  off_t filesize = (off_t)nbb * 4096;
  if (ftruncate(fd, filesize) < 0)
  {
    perror("ftruncate");
    return EXIT_FAILURE;
  }

  // Projection de tout le conteneur en mémoire
  void *map = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED)
  {
    perror("mmap");
    return EXIT_FAILURE;
  }

  // 1) Superbloc
  struct pignoufs *sb = (struct pignoufs *)map;
  memset(sb, 0, sizeof(*sb));
  memcpy(sb->magic, "pignoufs", 8);
  sb->nb_b = htole32(nbb);
  sb->nb_i = htole32(nbi);
  sb->nb_a = htole32(nba);
  sb->nb_l = htole32(nba + nbi);
  sb->nb_f = htole32(0);
  // SHA1 + type
  uint8_t *sha1 = (uint8_t *)map;
  calcul_sha1(sha1, 4000, sha1 + 4000);
  struct bitmap_block *bb = (struct bitmap_block *)(sha1 + 4000);
  bb->type = htole32(1);

  // 2) Blocs de bitmap
  for (int32_t i = 0; i < nb1; i++)
  {
    struct bitmap_block *bb = (struct bitmap_block *)((uint8_t *)map + (i + 1) * 4096);
    // Réinitialisation des bits
    memset(bb->bits, 0, sizeof(bb->bits));
    int32_t base = i * 32000;
    for (int j = 0; j < 32000; j++)
    {
      int32_t b = base + j;
      if (b >= 1 + nb1 && b < nbb)
        bb->bits[j / 8] |= (1 << (j % 8));
    }
    calcul_sha1(bb->bits, 4000, bb->sha1);
    bb->type = htole32(2);
  }

  // 3) Blocs d'inodes
  for (int32_t i = 0; i < nbi; i++)
  {
    struct inode *in = (struct inode *)((uint8_t *)map + (1 + nb1 + i) * 4096);
    memset(in, 0, sizeof(*in)); // marquer l'inode comme libre
    calcul_sha1(in, 4000, in->sha1);
    in->type = htole32(3);
  }

  // 4) Blocs de données
  for (int32_t i = 0; i < nba; i++)
  {
    struct data_block *db = (struct data_block *)((uint8_t *)map + (1 + nb1 + nbi + i) * 4096);
    memset(db->data, 0, sizeof(db->data));
    calcul_sha1(db->data, 4000, db->sha1);
    db->type = htole32(5);
  }

  msync(map, filesize, MS_SYNC);
  munmap(map, filesize);
  close(fd);

  printf("Système de fichiers %s créé avec %d inodes et %d blocs allouables.\n", fsname, nbi, nba);
  printf("Nombre total de blocs : %d\n", nbb);
}

void cmd_touch(int argc, char *argv[])
{
  const char *fsname = argv[1];
  const char *filename = argv[2];
  if (strncmp(filename, "//", 2) != 0)
  {
    fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
    return;
  }
  filename += 2;

  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);

  int32_t nb1, nbi, nba;
  get_conteneur_data(map, &nb1, &nbi, &nba);
  struct pignoufs *sb = (struct pignoufs *)map;
  int32_t base = 1 + nb1;
  int32_t free_idx = -1;

  // Scan inodes
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = (struct inode *)(map + (base + i) * 4096);
    if (in->flags & 1)
    {
      if (strcmp(in->filename, filename) == 0)
      {
        in->modification_time = time(NULL);
        calcul_sha1(in, 4000, in->sha1);
        close_fs(fd, map, size);
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
    close_fs(fd, map, size);
    return -1;
  }

  // Initialiser l'inode et l'allouer dans le bitmap
  struct inode *in = (struct inode *)(map + (base + free_idx) * 4096);
  init_inode(in, filename, false);
  int32_t inode_blk = base + free_idx;
  bitmap_alloc(map, inode_blk);

  // Mettre à jour les compteurs du superbloc et son SHA1
  sb->nb_l -= 1;
  sb->nb_f += 1;
  calcul_sha1(map, 4000, map + 4000);

  close_fs(fd, map, size);
  return 0;
}

void cmd_ls(int argc, char *argv[])
{
  const char *fsname = argv[1];
  const char *filename = NULL;
  if (argc > 2)
  {
    filename = argv[2];
    if (strncmp(filename, "//", 2) != 0)
    {
      fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
      return;
    }
    filename += 2;
  }

  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return -1;

  int32_t nb1, nbi, nba;
  get_conteneur_data(map, &nb1, &nbi, &nba);
  struct pignoufs *sb = (struct pignoufs *)map;
  int32_t base = 1 + nb1;
  int found = 0;

  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = (struct inode *)(map + (base + i) * 4096);
    if (in->flags & 1)
    {
      if (filename)
      {
        if (strcmp(in->filename, filename) == 0)
        {
          char perm[5] = {'-', '-', '-', '-', '\0'};
          perm[0] = (in->flags & (1 << 1)) ? 'r' : '-';
          perm[1] = (in->flags & (1 << 2)) ? 'w' : '-';
          perm[2] = (in->flags & (1 << 3)) ? 'l' : '-';
          perm[3] = (in->flags & (1 << 4)) ? 'l' : '-';

          time_t mod_time = (time_t)in->modification_time;
          struct tm tm_buf;
          struct tm *tm = localtime_r(&mod_time, &tm_buf);
          char buf[20] = "unknown time";
          if (tm)
          {
            strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", tm);
          }

          printf("%s %10u %s %s\n", perm, in->file_size, buf, filename);
          found = 1;
          break;
        }
      }
      else
      {
        printf("%s\n", in->filename);
      }
    }
  }

  if (filename && !found)
  {
    printf("%s introuvable\n", filename);
    close_fs(fd, map, size);
    return -1;
  }
  close_fs(fd, map, size);
  return 0;
}

void cmd_df(int argc, char *argv[])
{
  const char *fsname = argv[1];
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return -1;

  struct pignoufs *sb = (struct pignoufs *)map;
  printf("%d blocs de 4Kio libres\n", sb->nb_l);

  close_fs(fd, map, size);
  return 0;
}

void cmd_cp(int argc, char *argv[])
{
  printf("Exécution de la commande cp\n");
}

void cmd_rm(int argc, char *argv[])
{
  printf("Exécution de la commande rm\n");
}

void cmd_lock(int argc, char *argv[])
{
  printf("Exécution de la commande lock\n");
}

void cmd_chmod(int argc, char *argv[])
{
  printf("Exécution de la commande chmod\n");
}

void cmd_cat(int argc, char *argv[])
{
  printf("Exécution de la commande cat\n");
}

void cmd_input(int argc, char *argv[])
{
  printf("Exécution de la commande input\n");
}

void cmd_add(int argc, char *argv[])
{
  printf("Exécution de la commande add\n");
}

void cmd_addinput(int argc, char *argv[])
{
  printf("Exécution de la commande addinput\n");
}

void cmd_fsck(int argc, char *argv[])
{
  printf("Exécution de la commande fsck\n");
}

void cmd_mount(int argc, char *argv[])
{
  printf("Exécution de la commande mount\n");
}