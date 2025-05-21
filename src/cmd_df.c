#include "../include/structures.h"
#include "../include/utilitaires.h"

int cmd_df(const char *fsname)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");
  struct pignoufs *sb = (struct pignoufs *)map;
  printf("%d blocs allouables de libres\n", FROM_LE32(sb->nb_l));
  printf("%d inodes libres\n", FROM_LE32(sb->nb_i) - FROM_LE32(sb->nb_f));

  close_fs(fd, map, size);
  return 0;
}