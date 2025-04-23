#include "../include/structures.h"
#include <endian.h> // Ajout pour les conversions endian
#include <math.h> // Ajout pour utiliser ceil

static void compute_sha1(const void *data, size_t len, uint8_t *out) {
  SHA_CTX ctx;
  SHA1_Init(&ctx);
  SHA1_Update(&ctx, data, len);
  SHA1_Final(out, &ctx);
}

void cmd_mkfs(int argc, char *argv[])
{
  const char *fsname = argv[1];
  int32_t nbi = atoi(argv[2]);
  int32_t nba = atoi(argv[3]);

  // Calcul initial du nombre de blocs
  int32_t nb1 = (int32_t)ceil((1 + nbi + nba) / 32000.0);
  int32_t nbb = 1 + nb1 + nbi + nba;
  int32_t nb1_new;
  // Boucle pour recalculer jusqu'à convergence
  while (1) {
      nb1_new = (int32_t)ceil(nbb / 32000.0);
      if (nb1_new == nb1) {
          break;
      }
      nb1 = nb1_new;
      nbb = 1 + nb1 + nbi + nba;
  }

  // Création et dimensionnement du conteneur
  int fd = open(fsname, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd < 0) { perror("open"); return EXIT_FAILURE; }
  off_t filesize = (off_t)nbb * 4096;
  if (ftruncate(fd, filesize) < 0) { perror("ftruncate"); return EXIT_FAILURE; }

  // Projection de tout le conteneur en mémoire
  void *map = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED) { perror("mmap"); return EXIT_FAILURE; }

  // 1) Superbloc
  struct pignoufs *sb = (struct pignoufs *)map;
  memset(sb, 0, sizeof(*sb));
  memcpy(sb->magic, "pignoufs", 8);
  sb->nb_b = htole32(nbb);
  sb->nb_i = htole32(nbi);
  sb->nb_a = htole32(nba);
  sb->nb_l = htole32(nbi + nba);
  sb->nb_f = htole32(0);
  // SHA1 + type
  uint8_t *raw0 = (uint8_t *)map;
  compute_sha1(raw0, 4000, raw0 + 4000);
  struct bitmap_block *sig0 = (struct bitmap_block *)(raw0 + 4000);
  sig0->type = htole32(1);

  // 2) Blocs de bitmap
  for (int32_t i = 0; i < nb1; i++) {
      struct bitmap_block *bb = (struct bitmap_block *)((uint8_t *)map + (i + 1) * 4096);
      // Réinitialisation des bits
      memset(bb->bits, 0, sizeof(bb->bits));
      int32_t base = i * 32000;
      for (int j = 0; j < 32000; j++) {
          int32_t b = base + j;
          if (b >= 1 + nb1 && b < nbb)
              bb->bits[j/8] |= (1 << (j%8));
      }
      compute_sha1(bb->bits, 4000, bb->sha1);
      bb->type = htole32(2);
  }

  // 3) Blocs d'inodes
  for (int32_t i = 0; i < nbi; i++) {
      struct inode *in = (struct inode *)((uint8_t *)map + (1 + nb1 + i) * 4096);
      memset(in, 0, sizeof(*in)); // marquer l'inode comme libre
      compute_sha1(in, 4000, in->sha1);
      in->type = htole32(3);
  }

  // 4) Blocs de données
  for (int32_t i = 0; i < nba; i++) {
      struct data_block *db = (struct data_block *)((uint8_t *)map + (1 + nb1 + nbi + i) * 4096);
      memset(db->data, 0, sizeof(db->data));
      compute_sha1(db->data, 4000, db->sha1);
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
  printf("Exécution de la commande touch\n");
}

void cmd_ls(int argc, char *argv[])
{
  printf("Exécution de la commande ls\n");
}

void cmd_df(int argc, char *argv[])
{
  printf("Exécution de la commande df\n");
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