#include "../include/structures.h"
#include "../include/utilitaires.h"

int cmd_mkdir(const char *fsname, char *path)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  int val = create_directory(map, path);
  if (val == -1)
  {
    close_fs(fd, map, size);
    return print_error("Erreur lors de la création du répertoire");
  }

  close_fs(fd, map, size);
  return 0;
}