#include "../include/structures.h"
#include "../include/utilitaires.h"

// Fonction pour rechercher des fichiers ou dossiers selon différents critères
int cmd_find(const char *fsname, const char *type, const char *name, int days, const char *date_type)
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

  time_t now = time(NULL);

  // Parcours de tous les inodes
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = get_inode(map, 1 + nb1 + i);
    if (!(FROM_LE32(in->flags) & 1)) // Ignorer les inodes non allouées (bit 0)
      continue;

    // Verrouille l'inode en lecture
    int inode_offset = (1 + nb1) * 4096 + (int64_t)(in - (struct inode *)map);
    if (lock_block(fd, inode_offset, F_RDLCK) < 0)
    {
      print_error("Erreur lors du verrouillage de l'inode");
      close_fs(fd, map, size);
      return 1;
    }

    // Vérifie les permissions et les locks :
    // Bit 1 : lecture, Bit 2 : écriture, Bit 3 : lock lecture, Bit 4 : lock écriture
    // On n'affiche que si l'inode a le droit de lecture et n'est pas locké en lecture ou écriture
    if (!((FROM_LE32(in->flags) >> 1) & 1) || ((FROM_LE32(in->flags) >> 3) & 1) || ((FROM_LE32(in->flags) >> 4) & 1)) {
      unlock_block(fd, inode_offset);
      continue;
    }

    // Filtrer par type (fichier ou dossier)
    if (type)
    {
      bool is_dir = (FROM_LE32(in->flags) & (1 << 5)) != 0;
      if ((strcmp(type, "f") == 0 && is_dir) || (strcmp(type, "d") == 0 && !is_dir))
      {
        unlock_block(fd, inode_offset); // Déverrouiller l'inode
        continue;
      }
    }

    // Filtrer par nom (si le nom ne correspond pas, on passe)
    if (name && strstr(in->filename, name) == NULL)
    {
      unlock_block(fd, inode_offset); // Déverrouiller l'inode
      continue;
    }

    // Filtrer par date (ctime, mtime, atime)
    if (days > 0 && date_type)
    {
      time_t file_time = 0;
      if (strcmp(date_type, "ctime") == 0)
        file_time = FROM_LE32(in->creation_time);
      else if (strcmp(date_type, "mtime") == 0)
        file_time = FROM_LE32(in->modification_time);
      else if (strcmp(date_type, "atime") == 0)
        file_time = FROM_LE32(in->access_time);

      if (difftime(now, file_time) > days * 24 * 60 * 60)
      {
        unlock_block(fd, inode_offset); // Déverrouiller l'inode
        continue;
      }
    }

    // Affiche le nom du fichier/dossier correspondant
    printf("%s\n", in->filename);

    // Déverrouille l'inode
    unlock_block(fd, inode_offset);
  }

  // Ferme le système de fichiers
  close_fs(fd, map, size);
  return 0;
}