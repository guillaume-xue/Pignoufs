#include "../include/structures.h"
#include "../include/utilitaires.h"

int cmd_rm(const char *fsname, const char *filename)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  // Séparer le chemin parent et le nom du répertoire
  char parent_path[256], dir_name[256];
  split_path(filename, parent_path, dir_name);

  int val = -1;
  struct inode *in = NULL;
  struct inode *in2 = NULL;
  int val2 = -1;

  if (strlen(parent_path) == 0)
  {
    val = find_inode_racine(map, nb1, nbi, dir_name, false);
    if (val == -1)
    {
      close_fs(fd, map, size);
      return print_error("Erreur: répertoire inexistant");
    }
    in = get_inode(map, val);
  }
  else
  {
    val = find_inode_folder(map, nb1, nbi, parent_path);
    if (val == -1)
    {
      close_fs(fd, map, size);
      return print_error("Erreur: répertoire parent inexistant");
    }
    in = get_inode(map, val);
    val2 = find_file_folder_from_inode(map, in, dir_name, false);
    if (val2 == -1)
    {
      close_fs(fd, map, size);
      return print_error("Erreur: répertoire à supprimer inexistant");
    }
    in2 = get_inode(map, val2);
    delete_separte_inode(in, val2);
  }

  // Verrouiller l'inode
  int inode_offset = (1 + nb1) * 4096 + (int64_t)(in2 - (struct inode *)map);
  if (lock_block(fd, inode_offset, F_WRLCK) < 0)
  {
    print_error("Erreur lors du verrouillage de l'inode");
    close_fs(fd, map, size);
    return 1;
  }

  // Verrouiller les blocs de données associés
  for (int i = 0; i < 900; i++)
  {
    int32_t b = FROM_LE32(in2->direct_blocks[i]);
    if (b < 0)
      break;

    if (lock_block(fd, b * 4096, F_WRLCK) < 0)
    {
      print_error("Erreur lors du verrouillage d'un bloc de données");
      unlock_block(fd, inode_offset);
      close_fs(fd, map, size);
      return 1;
    }
    unlock_block(fd, b * 4096);
  }

  if ((int32_t)FROM_LE32(in2->double_indirect_block) >= 0)
  {
    struct address_block *dbl = get_address_block(map, FROM_LE32(in2->double_indirect_block));
    if (FROM_LE32(dbl->type) == 6)
    {
      for (int i = 0; i < 1000; i++)
      {
        int32_t db = FROM_LE32(dbl->addresses[i]);
        if (db < 0)
          break;

        if (lock_block(fd, db * 4096, F_WRLCK) < 0)
        {
          print_error("Erreur lors du verrouillage d'un bloc de données indirect");
          unlock_block(fd, inode_offset);
          close_fs(fd, map, size);
          return 1;
        }
        unlock_block(fd, db * 4096);
      }
    }
    else
    {
      for (int i = 0; i < 1000; i++)
      {
        int32_t db = FROM_LE32(dbl->addresses[i]);
        if (db < 0)
          break;

        struct address_block *sib = get_address_block(map, db);
        for (int j = 0; j < 1000; j++)
        {
          int32_t db2 = FROM_LE32(sib->addresses[j]);
          if (db2 < 0)
            break;

          if (lock_block(fd, db2 * 4096, F_WRLCK) < 0)
          {
            print_error("Erreur lors du verrouillage d'un bloc de données double indirect");
            unlock_block(fd, inode_offset);
            close_fs(fd, map, size);
            return 1;
          }
          unlock_block(fd, db2 * 4096);
        }
      }
    }
  }

  // Déverrouiller l'inode
  unlock_block(fd, inode_offset);

  close_fs(fd, map, size);
  return 0;
}