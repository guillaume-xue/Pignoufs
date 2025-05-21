#include "../include/structures.h"
#include "../include/utilitaires.h"

int cmd_input(const char *fsname, const char *filename)
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

  if (strlen(parent_path) == 0)
  {
    val = find_inode_racine(map, nb1, nbi, dir_name, false);
    if (val == -1)
    {
      val = create_file(map, dir_name);
    }
    in = get_inode(map, val);
  }
  else
  {
    val = find_inode_folder(map, nb1, nbi, parent_path);
    if (val == -1)
    {
      print_error("Erreur: répertoire inexistant, création manuellement");
      val = create_directory(map, parent_path);
      in = get_inode(map, val);
      val = create_file(map, dir_name);
      struct inode *in2 = get_inode(map, val);
      in2->profondeur = TO_LE32(FROM_LE32(in->profondeur) + 1);
      add_inode(in, val);
      in = in2;
    }
    else
    {
      in = get_inode(map, val);
      val = find_file_folder_from_inode(map, in, dir_name, false);
      if (val == -1)
      {
        val = create_file(map, dir_name);
        struct inode *in2 = get_inode(map, val);
        in2->profondeur = TO_LE32(FROM_LE32(in->profondeur) + 1);
        add_inode(in, val);
        in = in2;
      }
      else
      {
        in = get_inode(map, val);
        dealloc_data_block(in, map, fd, size);
      }
    }
  }
  // Verrouiller l'inode
  int inode_offset = (1 + nb1) * 4096 + (int64_t)(in - (struct inode *)map);
  if (lock_block(fd, inode_offset, F_WRLCK) < 0)
  {
    print_error("Erreur lors du verrouillage de l'inode");
    close_fs(fd, map, size);
    return 1;
  }
  char buf[4000];
  size_t r;
  uint32_t total = FROM_LE32(in->file_size);
  while ((r = read(STDIN_FILENO, buf, 4000)) > 0)
  {
    uint32_t offset = (total % 4000);
    if (offset > 0)
    {
      int32_t last = get_last_data_block(map, in);
      struct data_block *db = get_data_block(map, last);

      // Verrouiller le bloc de données
      int data_offset = last * 4096;
      if (lock_block(fd, data_offset, F_WRLCK) < 0)
      {
        print_error("Erreur lors du verrouillage du bloc de données");
        close_fs(fd, map, size);
        return 1;
      }

      memcpy(db->data + offset, buf, r);
      calcul_sha1(db->data, 4000, db->sha1);
      total += r;
      in->file_size = TO_LE32(total);

      // Déverrouiller le bloc de données
      unlock_block(fd, data_offset);
    }
    else
    {
      int32_t b = get_last_data_block_null(map, in);
      if (b == -2)
      {
        print_error("Erreur: pas de blocs de données libres disponibles");
        break;
      }
      struct data_block *db = get_data_block(map, b);

      // Verrouiller le bloc de données
      int data_offset = b * 4096;
      if (lock_block(fd, data_offset, F_WRLCK) < 0)
      {
        print_error("Erreur lors du verrouillage du bloc de données");
        close_fs(fd, map, size);
        return 1;
      }

      memset(db->data, 0, sizeof(db->data));
      memcpy(db->data, buf, r);
      calcul_sha1(db->data, 4000, db->sha1);
      db->type = TO_LE32(5);
      total += r;
      in->file_size = TO_LE32(total);

      // Déverrouiller le bloc de données
      unlock_block(fd, data_offset);
    }
  }
  in->file_size = TO_LE32(total);
  in->modification_time = TO_LE32(time(NULL));
  calcul_sha1(in, 4000, in->sha1);

  // Déverrouiller l'inode
  unlock_block(fd, inode_offset);

  close_fs(fd, map, size);
  return 0;
}
