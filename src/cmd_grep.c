#include "../include/structures.h"
#include "../include/utilitaires.h"

// Fonction pour rechercher un motif dans tous les fichiers du système de fichiers interne
int cmd_grep(const char *fsname, const char *pattern)
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

  // Parcours de tous les inodes
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = get_inode(map, 1 + nb1 + i);
    if (!(FROM_LE32(in->flags) & 1)) // Ignorer les inodes non allouées (bit 0)
      continue;

    // Vérifie les permissions et les locks :
    // On ne lit que si l'inode a le droit de lecture et n'est pas locké en lecture ou écriture
    if (!((FROM_LE32(in->flags) >> 1) & 1) || ((FROM_LE32(in->flags) >> 3) & 1) || ((FROM_LE32(in->flags) >> 4) & 1))
      continue;

    // Lire le contenu du fichier interne
    uint32_t file_size = FROM_LE32(in->file_size);
    char *content = malloc(file_size + 1);
    if (!content)
    {
      print_error("Erreur d'allocation mémoire");
      close_fs(fd, map, size);
      return 1;
    }

    uint32_t remaining_data = file_size;
    char *ptr = content;

    // Parcours des blocs directs pour lire le contenu
    for (int j = 0; j < 900 && remaining_data > 0; j++)
    {
      int32_t b = FROM_LE32(in->direct_blocks[j]);
      if (b < 0)
        break;

      // Verrouille le bloc de données pour lecture
      if (lock_block(fd, b * 4096, F_RDLCK) < 0)
      {
        print_error("Erreur lors du verrouillage du bloc de données");
        free(content);
        close_fs(fd, map, size);
        return 1;
      }

      struct data_block *data_position = get_data_block(map, b);
      uint32_t buffer = remaining_data < 4000 ? remaining_data : 4000;
      memcpy(ptr, data_position->data, buffer);
      ptr += buffer;
      remaining_data -= buffer;

      // Déverrouille le bloc de données
      unlock_block(fd, b * 4096);
    }
    content[file_size] = '\0';

    // Recherche du motif dans le contenu du fichier
    if (strstr(content, pattern) != NULL)
      printf("%s\n", in->filename);

    free(content);
  }

  // Ferme le système de fichiers
  close_fs(fd, map, size);
  return 0;
}