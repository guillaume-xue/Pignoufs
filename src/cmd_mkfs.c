#include "../include/structures.h"
#include "../include/utilitaires.h"

// Fonction de création du système de fichiers (mkfs)
int cmd_mkfs(const char *fsname, int nbi, int nba)
{
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
    return print_error("open: erreur d'ouverture du fichier");
  }
  int64_t filesize = (int64_t)nbb * 4096;

  if (ftruncate(fd, filesize) < 0)
  {
    print_error("ftruncate: erreur");
    close(fd);
    return 1;
  }

  // Projection de tout le conteneur en mémoire
  void *map = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED)
  {
    print_error("mmap: erreur");
    close(fd);
    return 1;
  }

  for (int32_t i = 0; i < nbb; i++)
  {
    calcul_sha1(map + (int64_t)i * 4096, 4000, ((uint8_t *)map + (int64_t)i * 4096) + 4000);
  }

  // 1) Superbloc
  struct pignoufs *sb = (struct pignoufs *)map;
  memset(sb, 0, sizeof(*sb));
  memcpy(sb->magic, "pignoufs", 8);
  sb->nb_b = TO_LE32(nbb);
  sb->nb_i = TO_LE32(nbi);
  sb->nb_a = TO_LE32(nba);
  sb->nb_l = TO_LE32(nba);
  sb->nb_f = TO_LE32(0);
  calcul_sha1(sb, 4000, sb->sha1);
  sb->type = TO_LE32(1);

  // 2) Blocs de bitmap
  for (int32_t i = 0; i < nb1; i++)
  {
    struct bitmap_block *bb = (struct bitmap_block *)((uint8_t *)map + (int64_t)(i + 1) * 4096);
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
    bb->type = TO_LE32(2);
  }

  // 3) Blocs d'inodes
  for (int32_t i = 0; i < nbi; i++)
  {
    struct inode *in = get_inode(map, 1 + nb1 + i);
    // Initialise chaque inode à zéro (libre)
    memset(in, 0, sizeof(*in)); // marquer l'inode comme libre
    calcul_sha1(in, 4000, in->sha1);
    in->type = TO_LE32(3);
  }

  // 4) Blocs de données
  for (int32_t i = 0; i < nba; i++)
  {
    struct data_block *db = get_data_block(map, 1 + nb1 + nbi + i);
    // Initialise chaque bloc de données à zéro
    memset(db->data, 0, sizeof(db->data));
    calcul_sha1(db->data, 4000, db->sha1);
    db->type = TO_LE32(4);
  }

  // Synchronise et libère la mémoire mappée
  msync(map, filesize, MS_SYNC);
  int er = munmap(map, filesize);
  if (er < 0)
  {
    print_error("munmap: erreur");
    close(fd);
    return 1;
  }
  close(fd);

  // Affiche un résumé de la création du système de fichiers
  printf("Système de fichiers %s créé avec %d inodes et %d blocs allouables.\n", fsname, nbi, nba);
  printf("Nombre total de blocs : %d\n", nbb);
  return 0;
}