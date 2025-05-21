#include "../include/structures.h"
#include "../include/utilitaires.h"

int cmd_grep(const char *fsname, const char *pattern)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = get_inode(map, 1 + nb1 + i);
    if (!(FROM_LE32(in->flags) & 1)) // Ignorer les inodes non allouées
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

    for (int j = 0; j < 900 && remaining_data > 0; j++)
    {
      int32_t b = FROM_LE32(in->direct_blocks[j]);
      if (b < 0)
        break;

      // Verrouiller le bloc de données
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

      // Déverrouiller le bloc de données
      unlock_block(fd, b * 4096);
    }
    content[file_size] = '\0';

    // Rechercher le motif
    if (strstr(content, pattern) != NULL)
      printf("%s\n", in->filename);

    free(content);
  }

  close_fs(fd, map, size);
  return 0;
}