#include "../include/structures.h"
#include "../include/utilitaires.h"

int cmd_chmod(const char *fsname, const char *filename, const char *arg)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  char parent_path[256], dir_name[256];
  split_path(filename, parent_path, dir_name);
  struct inode *in = NULL;
  int val = -1;
  bool is_dir = is_folder_path(parent_path, dir_name);

  if (is_dir)
  {
    split_path(parent_path, parent_path, dir_name);
  }
  if (strlen(parent_path) == 0)
  {
    val = find_inode_racine(map, nb1, nbi, dir_name, false);
    if (val < 0)
    {
      print_error("Fichier introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
  }
  else
  {
    val = find_inode_folder(map, nb1, nbi, parent_path);
    if (val < 0)
    {
      print_error("Répertoire introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
    val = find_file_folder_from_inode(map, in, dir_name, false);
    if (val < 0)
    {
      print_error("Fichier introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
  }

  // Verrouiller l'inode
  int inode_offset = (1 + nb1) * 4096 + (int64_t)(in - (struct inode *)map);
  if (lock_block(fd, inode_offset, F_WRLCK) < 0)
  {
    print_error("Erreur lors du verrouillage de l'inode");
    close_fs(fd, map, size);
    return 1;
  }

  // Modifier les permissions
  if (strcmp(arg, "+r") == 0)
  {
    in->flags |= (1 << 1);
  }
  else if (strcmp(arg, "-r") == 0)
  {
    in->flags &= ~(1 << 1);
  }
  else if (strcmp(arg, "+w") == 0)
  {
    in->flags |= (1 << 2);
  }
  else if (strcmp(arg, "-w") == 0)
  {
    in->flags &= ~(1 << 2);
  }
  else
  {
    print_error("Argument invalide");
    unlock_block(fd, inode_offset);
    close_fs(fd, map, size);
    return 1;
  }

  in->modification_time = TO_LE32(time(NULL));
  calcul_sha1(in, 4000, in->sha1);

  // Déverrouiller l'inode
  unlock_block(fd, inode_offset);

  close_fs(fd, map, size);
  return 0;
}