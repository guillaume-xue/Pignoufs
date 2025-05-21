#include "../include/structures.h"
#include "../include/utilitaires.h"

static void copy_interne(uint8_t *map, struct inode *in, struct inode *in2)
{
  for (int i = 0; i < 900; i++)
  {
    if (in->direct_blocks[i] != -1)
    {
      struct data_block *data_position = get_data_block(map, in->direct_blocks[i]);
      int32_t new_block = alloc_data_block(map);
      struct data_block *new_data_position = get_data_block(map, new_block);
      memcpy(new_data_position->data, data_position->data, 4000);
      in2->direct_blocks[i] = TO_LE32(new_block);
      calcul_sha1(new_data_position->data, 4000, new_data_position->sha1);
    }
  }
  if ((int32_t)FROM_LE32(in->double_indirect_block) != -1)
  {
    struct address_block *dbl = get_address_block(map, FROM_LE32(in->double_indirect_block));
    if ((int32_t)FROM_LE32(dbl->type) == 6)
    {
      in2->double_indirect_block = TO_LE32(alloc_data_block(map));
      struct address_block *dbl2 = get_address_block(map, FROM_LE32(in2->double_indirect_block));
      memset(dbl2->addresses, -1, sizeof(dbl2->addresses));
      calcul_sha1(dbl2->addresses, 4000, dbl2->sha1);
      calcul_sha1(in2, 4000, in2->sha1);
      dbl2->type = TO_LE32(6);
      for (int i = 0; i < 1000; i++)
      {
        if (dbl->addresses[i] != -1)
        {
          struct data_block *data_position = get_data_block(map, dbl->addresses[i]);
          int32_t new_block = alloc_data_block(map);
          struct data_block *new_data_position = get_data_block(map, new_block);
          memcpy(new_data_position->data, data_position->data, 4000);
          dbl2->addresses[i] = TO_LE32(new_block);
          calcul_sha1(new_data_position->data, 4000, new_data_position->sha1);
        }
      }
      calcul_sha1(dbl2->addresses, 4000, dbl2->sha1);
    }
    else
    {
      in2->double_indirect_block = TO_LE32(alloc_data_block(map));
      struct address_block *dbl2 = get_address_block(map, FROM_LE32(in2->double_indirect_block));
      memset(dbl2->addresses, -1, sizeof(dbl2->addresses));
      calcul_sha1(in2, 4000, in2->sha1);
      dbl2->type = TO_LE32(7);

      for (int i = 0; i < 1000; i++)
      {
        if (dbl->addresses[i] != -1)
        {
          struct address_block *sib = get_address_block(map, FROM_LE32(dbl->addresses[i]));
          int32_t new_block = alloc_data_block(map);
          struct address_block *dbl3 = get_address_block(map, new_block);
          memset(dbl3->addresses, -1, sizeof(dbl3->addresses));

          dbl3->type = TO_LE32(6);
          for (int j = 0; j < 1000; j++)
          {
            if (sib->addresses[j] != -1)
            {
              struct data_block *data_position = get_data_block(map, sib->addresses[j]);
              int32_t new_block = alloc_data_block(map);
              struct data_block *new_data_position = get_data_block(map, new_block);
              memcpy(new_data_position->data, data_position->data, 4000);
              dbl3->addresses[j] = TO_LE32(new_block);
              calcul_sha1(new_data_position->data, 4000, new_data_position->sha1);
            }
          }
          dbl2->addresses[i] = TO_LE32(new_block);
          calcul_sha1(dbl3->addresses, 4000, dbl3->sha1);
        }
      }
      calcul_sha1(dbl2->addresses, 4000, dbl2->sha1);
    }
  }
}

static void copy_interne_main(uint8_t *map, struct inode *in, struct inode *in2)
{
  int32_t new_block = alloc_data_block(map);
  struct inode *dossier = get_inode(map, new_block);
  dossier->profondeur = TO_LE32(FROM_LE32(in2->profondeur) + 1);
  init_inode(dossier, in->filename, true);
  copy_interne(map, in, dossier);
  add_inode(in2, new_block);
  calcul_sha1(dossier, 4000, dossier->sha1);
}

static void copy_dossier(uint8_t *map, struct inode *in, struct inode *in2)
{
  for (int i = 0; i < 900; i++)
  {
    if (in->direct_blocks[i] != -1)
    {
      struct inode *dossier = get_inode(map, in->direct_blocks[i]);
      if ((FROM_LE32(dossier->flags) >> 5) & 1)
      {
        int32_t new_block = alloc_data_block(map);
        struct inode *dossier2 = get_inode(map, new_block);
        dossier2->profondeur = TO_LE32(FROM_LE32(in2->profondeur) + 1);
        init_inode(dossier2, dossier->filename, true);
        copy_dossier(map, dossier, dossier2);
        add_inode(in2, new_block);
        calcul_sha1(dossier2, 4000, dossier2->sha1);
      }
      else
      {
        struct inode *fichier = get_inode(map, in->direct_blocks[i]);
        int32_t new_block = alloc_data_block(map);
        init_inode(fichier, dossier->filename, false);
        struct inode *fichier2 = get_inode(map, new_block);
        fichier2->profondeur = TO_LE32(FROM_LE32(in2->profondeur) + 1);
        init_inode(fichier2, fichier->filename, false);
        copy_interne(map, fichier, fichier2);
        add_inode(in2, new_block);
        calcul_sha1(fichier2, 4000, fichier2->sha1);
      }
    }
  }
}

static void copy_dossier_main(uint8_t *map, struct inode *in, struct inode *in2)
{
  int32_t new_block = alloc_data_block(map);
  struct inode *dossier = get_inode(map, new_block);
  dossier->profondeur = TO_LE32(FROM_LE32(in2->profondeur) + 1);
  init_inode(dossier, in->filename, true);
  copy_dossier(map, in, dossier);
  add_inode(in2, new_block);
  calcul_sha1(dossier, 4000, dossier->sha1);
}

static void copy_fichier_vers_externe(uint8_t *map, struct inode *in, const char *filename)
{
  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0)
  {
    perror("Erreur d'ouverture du fichier externe");
    return;
  }

  uint32_t file_size = FROM_LE32(in->file_size);
  uint32_t bytes_written = 0;

  // Copier les blocs directs
  for (int i = 0; i < 900 && bytes_written < file_size; i++)
  {
    int32_t blk = FROM_LE32(in->direct_blocks[i]);
    if (blk == -1)
      break;
    struct data_block *db = get_data_block(map, blk);
    uint32_t to_write = (file_size - bytes_written > 4000) ? 4000 : (file_size - bytes_written);
    if (write(fd, db->data, to_write) != to_write)
    {
      perror("Erreur d'écriture dans le fichier externe");
      close(fd);
      return;
    }
    bytes_written += to_write;
  }

  // Copier les blocs indirects/double indirects si besoin
  if (bytes_written < file_size && (int32_t)FROM_LE32(in->double_indirect_block) != -1)
  {
    struct address_block *dbl = get_address_block(map, FROM_LE32(in->double_indirect_block));
    if (FROM_LE32(dbl->type) == 6)
    {
      // Simple indirect
      for (int i = 0; i < 1000 && bytes_written < file_size; i++)
      {
        int32_t blk = FROM_LE32(dbl->addresses[i]);
        if (blk == -1)
          break;
        struct data_block *db = get_data_block(map, blk);
        uint32_t to_write = (file_size - bytes_written > 4000) ? 4000 : (file_size - bytes_written);
        if (write(fd, db->data, to_write) != to_write)
        {
          perror("Erreur d'écriture dans le fichier externe");
          close(fd);
          return;
        }
        bytes_written += to_write;
      }
    }
    else
    {
      // Double indirect
      for (int i = 0; i < 1000 && bytes_written < file_size; i++)
      {
        int32_t sib_blk = FROM_LE32(dbl->addresses[i]);
        if (sib_blk == -1)
          break;
        struct address_block *sib = get_address_block(map, sib_blk);
        for (int j = 0; j < 1000 && bytes_written < file_size; j++)
        {
          int32_t blk = FROM_LE32(sib->addresses[j]);
          if (blk == -1)
            break;
          struct data_block *db = get_data_block(map, blk);
          uint32_t to_write = (file_size - bytes_written > 4000) ? 4000 : (file_size - bytes_written);
          if (write(fd, db->data, to_write) != to_write)
          {
            perror("Erreur d'écriture dans le fichier externe");
            close(fd);
            return;
          }
          bytes_written += to_write;
        }
      }
    }
  }
  close(fd);
}

static void copy_dossier_vers_externe(uint8_t *map, struct inode *in, const char *dirname)
{
  // Créer le dossier externe
  if (mkdir(dirname, 0755) < 0 && errno != EEXIST)
  {
    perror("Erreur lors de la création du dossier externe");
    return;
  }

  char path[512];
  for (int i = 0; i < 900; i++)
  {
    int32_t child_idx = FROM_LE32(in->direct_blocks[i]);
    if (child_idx == -1)
      continue;
    struct inode *child = get_inode(map, child_idx);

    snprintf(path, sizeof(path), "%s/%s", dirname, child->filename);

    if (((FROM_LE32(child->flags) >> 5) & 1))
    {
      // C'est un dossier
      copy_dossier_vers_externe(map, child, path);
    }
    else
    {
      // C'est un fichier
      copy_fichier_vers_externe(map, child, path);
    }
  }
}

static void copy_fichier_vers_interne(uint8_t *map, struct inode *in, const char *filename)
{
  int fd = open(filename, O_RDONLY);
  if (fd < 0)
  {
    perror("Erreur d'ouverture du fichier externe");
    return;
  }

  char buf[4000];
  size_t r;
  uint32_t total = 0;

  while ((r = read(fd, buf, 4000)) > 0)
  {
    int32_t b = get_last_data_block_null(map, in);
    if (b == -2)
    {
      print_error("Erreur: pas de blocs de données libres disponibles");
      break;
    }

    struct data_block *db = get_data_block(map, b);
    memset(db->data, 0, sizeof(db->data));
    memcpy(db->data, buf, r);
    calcul_sha1(db->data, 4000, db->sha1);
    db->type = TO_LE32(5);
    total += r;
    in->file_size = TO_LE32(total);
  }
  in->modification_time = TO_LE32(time(NULL));
  calcul_sha1(in, 4000, in->sha1);

  close(fd);
}

static void copy_dossier_vers_interne(uint8_t *map, struct inode *in, const char *dirname)
{
  DIR *dir = opendir(dirname);
  if (!dir)
  {
    perror("Erreur d'ouverture du dossier externe");
    return;
  }

  struct dirent *entry;
  char path[512];
  while ((entry = readdir(dir)) != NULL)
  {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);

    struct stat st;
    if (stat(path, &st) < 0)
    {
      perror("Erreur stat");
      continue;
    }

    if (S_ISDIR(st.st_mode))
    {
      // Créer un inode dossier dans le FS interne
      int32_t new_blk = alloc_data_block(map);
      struct inode *new_dir = get_inode(map, new_blk);
      init_inode(new_dir, entry->d_name, true);
      new_dir->profondeur = TO_LE32(FROM_LE32(in->profondeur) + 1);
      add_inode(in, new_blk);
      copy_dossier_vers_interne(map, new_dir, path);
    }
    else if (S_ISREG(st.st_mode))
    {
      // Créer un inode fichier dans le FS interne
      int32_t new_blk = alloc_data_block(map);
      struct inode *new_file = get_inode(map, new_blk);
      init_inode(new_file, entry->d_name, false);
      new_file->profondeur = TO_LE32(FROM_LE32(in->profondeur) + 1);
      add_inode(in, new_blk);
      copy_fichier_vers_interne(map, new_file, path);
    }
  }
  closedir(dir);
}

int cmd_cp(const char *fsname, const char *filename1, const char *filename2, bool mode1, bool mode2)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  bool is_dir, is_dir2;
  char parent_path[256], dir_name[256];
  char parent_path2[256], dir_name2[256];
  int val = -1;
  int val2 = -1;
  struct inode *in = NULL;
  struct inode *in2 = NULL;

  struct stat st;

  if (mode1)
  {
    split_path(filename1, parent_path, dir_name);
    is_dir = is_folder_path(parent_path, dir_name);
  }
  else
  {
    if (stat(filename1, &st) == -1)
    {
      close_fs(fd, map, size);
      return print_error("Erreur: impossible d'accéder au fichier externe");
    }
  }
  if (mode2)
  {
    split_path(filename2, parent_path2, dir_name2);
    is_dir2 = is_folder_path(parent_path2, dir_name2);
  }
  else
  {
    if (stat(filename1, &st) == -1)
    {
      close_fs(fd, map, size);
      return print_error("Erreur: impossible d'accéder au fichier externe");
    }
  }

  if (mode1 && !mode2) // interne vers externe
  {
    if (is_dir)
    {
      val = find_inode_folder(map, nb1, nbi, parent_path);
      if (val == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire parent inexistant");
      }
      in = get_inode(map, val);
    }
    else
    {
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
        val = find_file_folder_from_inode(map, in, dir_name, false);
        if (val == -1)
        {
          close_fs(fd, map, size);
          return print_error("Erreur: répertoire à supprimer inexistant");
        }
        in = get_inode(map, val);
      }

      if ((in->flags) >> 5 & 1)
      {
        copy_dossier_vers_externe(map, in, filename1);
      }
      else
      {
        copy_fichier_vers_externe(map, in, filename1);
      }
    }
  }
  else if (!mode1 && mode2) // externe vers interne
  {
    if (is_dir2)
    {
      val = find_inode_folder(map, nb1, nbi, parent_path2);
      if (val == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire parent inexistant");
      }
      in = get_inode(map, val);
    }
    else
    {
      if (strlen(parent_path2) == 0)
      {
        val = find_inode_racine(map, nb1, nbi, dir_name2, false);
        if (val == -1)
        {
          close_fs(fd, map, size);
          return print_error("Erreur: répertoire inexistant");
        }
        in = get_inode(map, val);
      }
      else
      {
        val = find_inode_folder(map, nb1, nbi, parent_path2);
        if (val == -1)
        {
          close_fs(fd, map, size);
          return print_error("Erreur: répertoire parent inexistant");
        }
        in = get_inode(map, val);
        val = find_file_folder_from_inode(map, in, dir_name2, false);
        if (val == -1)
        {
          close_fs(fd, map, size);
          return print_error("Erreur: répertoire à supprimer inexistant");
        }
        in = get_inode(map, val);
      }
    }

    if (val == -1)
    {
      close_fs(fd, map, size);
      return print_error("Erreur: répertoire parent inexistant");
    }
    in = get_inode(map, val);
    if (val == -1)
    {
      close_fs(fd, map, size);
      return print_error("Erreur: répertoire parent inexistant");
    }

    if (S_ISDIR(st.st_mode))
    {
      copy_dossier_vers_interne(map, in, filename1);
    }
    else
    {
      copy_fichier_vers_interne(map, in, filename1);
    }
  }
  else if (mode1 && mode2) // interne vers interne
  {
    if (is_dir)
    {
      val = find_inode_folder(map, nb1, nbi, parent_path);
      if (val == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire parent inexistant");
      }
      in = get_inode(map, val);
    }
    else
    {
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
        val = find_file_folder_from_inode(map, in, dir_name, false);
        if (val == -1)
        {
          close_fs(fd, map, size);
          return print_error("Erreur: répertoire à supprimer inexistant");
        }
        in = get_inode(map, val);
      }
    }

    if (is_dir2)
    {
      val2 = find_inode_folder(map, nb1, nbi, parent_path2);
      if (val2 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire parent inexistant");
      }
      in2 = get_inode(map, val2);
    }
    else
    {
      if (strlen(parent_path2) == 0)
      {
        val2 = find_inode_racine(map, nb1, nbi, dir_name2, false);
        if (val2 == -1)
        {
          close_fs(fd, map, size);
          return print_error("Erreur: répertoire inexistant");
        }
        in2 = get_inode(map, val2);
      }
      else
      {
        val2 = find_inode_folder(map, nb1, nbi, parent_path2);
        if (val2 == -1)
        {
          close_fs(fd, map, size);
          return print_error("Erreur: répertoire parent inexistant");
        }
        in2 = get_inode(map, val2);
        val2 = find_file_folder_from_inode(map, in2, dir_name2, false);
        if (val2 == -1)
        {
          close_fs(fd, map, size);
          return print_error("Erreur: répertoire à supprimer inexistant");
        }
        in2 = get_inode(map, val2);
      }
    }

    if (is_dir && is_dir2)
    {
      copy_dossier_main(map, in, in2);
    }
    else if (!is_dir && is_dir2)
    {
      copy_interne_main(map, in, in2);
    }
  }
  return 0;
}