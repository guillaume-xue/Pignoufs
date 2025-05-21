#include "../include/structures.h"
#include "../include/utilitaires.h"

// Fonction pour afficher l'espace libre (blocs et inodes) du système de fichiers
int cmd_df(const char *fsname)
{
  uint8_t *map;
  size_t size;
  // Ouvre le système de fichiers et mappe son contenu en mémoire
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  // Récupère le superbloc pour obtenir les informations de capacité
  struct pignoufs *sb = (struct pignoufs *)map;
  // Affiche le nombre de blocs libres
  printf("%d blocs allouables de libres\n", FROM_LE32(sb->nb_l));
  // Affiche le nombre d'inodes libres
  printf("%d inodes libres\n", FROM_LE32(sb->nb_i) - FROM_LE32(sb->nb_f));

  // Ferme le système de fichiers
  close_fs(fd, map, size);
  return 0;
}