#include "../include/structures.h"
#include "../include/utilitaires.h"

// Fonction pour créer un répertoire dans le système de fichiers interne
int cmd_mkdir(const char *fsname, char *path)
{
  uint8_t *map;
  size_t size;
  // Ouvre le système de fichiers et mappe son contenu en mémoire
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  // Récupère les informations sur la structure du conteneur
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  // Crée le répertoire demandé
  int val = create_directory(map, path);
  if (val == -1)
  {
    close_fs(fd, map, size);
    return print_error("Erreur lors de la création du répertoire");
  }
  // Ferme le système de fichiers
  close_fs(fd, map, size);
  return 0;
}