#include "../include/structures.h"
#include "../include/utilitaires.h"

volatile sig_atomic_t keep_running = 1;
void handle_sigterm(int sig)
{
  (void)sig;
  keep_running = 0;
}
void handle_sigint(int sig)
{
  (void)sig;
  raise(SIGTERM);
}

// Fonction de création du système de fichiers (mkfs)
int cmd_mkfs(const char *fsname, int nbi, int nba)
{
  // Calcul initial du nombre de blocs
  int32_t nb1 = (int32_t)ceil((1 + nbi + nba) / 32000.0);
  int32_t nbb = 1 + nb1 + nbi + nba;
  int32_t nb1_new;
  // Boucle pour recalculer jusqu'à convergence
  while (1)
  {
    nb1_new = (int32_t)ceil(nbb / 32000.0);
    if (nb1_new == nb1)
    {
      break;
    }
    nb1 = nb1_new;
    nbb = 1 + nb1 + nbi + nba;
  }
  // Création et dimensionnement du conteneur
  int fd = open(fsname, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
  {
    return print_error("open: erreur d'ouverture du fichier");
  }
  int64_t filesize = (int64_t)nbb * 4096;

  if (ftruncate(fd, filesize) < 0)
  {
    print_error("ftruncate: erreur");
    close(fd);
    return 1;
  }

  // Projection de tout le conteneur en mémoire
  void *map = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED)
  {
    print_error("mmap: erreur");
    close(fd);
    return 1;
  }

  // 1) Superbloc
  struct pignoufs *sb = (struct pignoufs *)map;
  memset(sb, 0, sizeof(*sb));
  memcpy(sb->magic, "pignoufs", 8);
  sb->nb_b = TO_LE32(nbb);
  sb->nb_i = TO_LE32(nbi);
  sb->nb_a = TO_LE32(nba);
  sb->nb_l = TO_LE32(nba);
  sb->nb_f = TO_LE32(0);
  calcul_sha1(sb, 4000, sb->sha1);
  sb->type = TO_LE32(1);

  // 2) Blocs de bitmap
  for (int32_t i = 0; i < nb1; i++)
  {
    struct bitmap_block *bb = (struct bitmap_block *)((uint8_t *)map + (int64_t)(i + 1) * 4096);
    // Réinitialisation des bits
    memset(bb->bits, 0, sizeof(bb->bits));
    int32_t base = i * 32000;
    for (int j = 0; j < 32000; j++)
    {
      int32_t b = base + j;
      if (b >= 1 + nb1 && b < nbb)
        bb->bits[j / 8] |= (1 << (j % 8));
    }
    calcul_sha1(bb->bits, 4000, bb->sha1);
    bb->type = TO_LE32(2);
  }

  // 3) Blocs d'inodes
  for (int32_t i = 0; i < nbi; i++)
  {
    struct inode *in = get_inode(map, 1 + nb1, i);
    memset(in, 0, sizeof(*in)); // marquer l'inode comme libre
    calcul_sha1(in, 4000, in->sha1);
    in->type = TO_LE32(3);
  }

  // 4) Blocs de données
  for (int32_t i = 0; i < nba; i++)
  {
    struct data_block *db = get_data_block(map, 1 + nb1 + nbi + i);
    memset(db->data, 0, sizeof(db->data));
    calcul_sha1(db->data, 4000, db->sha1);
    db->type = TO_LE32(4);
  }

  msync(map, filesize, MS_SYNC);
  int er = munmap(map, filesize);
  if (er < 0)
  {
    print_error("munmap: erreur");
    close(fd);
    return 1;
  }
  close(fd);

  printf("Système de fichiers %s créé avec %d inodes et %d blocs allouables.\n", fsname, nbi, nba);
  printf("Nombre total de blocs : %d\n", nbb);
  return 0;
}

int cmd_ls(const char *fsname, const char *filename)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");
  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  int found = 0;
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = get_inode(map, 1 + nb1, i);
    if (in->flags & 1)
    {
      if (filename)
      {
        if (strcmp(in->filename, filename) == 0)
        {
          char perm[5] = {'-', '-', '-', '-', '\0'};
          perm[0] = (in->flags & (1 << 1)) ? 'r' : '-';
          perm[1] = (in->flags & (1 << 2)) ? 'w' : '-';
          perm[2] = (in->flags & (1 << 3)) ? 'l' : '-';
          perm[3] = (in->flags & (1 << 4)) ? 'l' : '-';
          time_t mod_time = (time_t)in->modification_time;
          struct tm tm_buf;
          struct tm *tm = localtime_r(&mod_time, &tm_buf);
          char buf[20] = "unknown time";
          if (tm)
          {
            strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", tm);
          }
          printf("%s %10u %s %s\n", perm, in->file_size, buf, filename);
          found = 1;
          break;
        }
      }
      else
      {
        printf("%s\n", in->filename);
      }
    }
  }

  if (filename && !found)
  {
    print_error("Fichier introuvable");
    close_fs(fd, map, size);
    return 1;
  }
  close_fs(fd, map, size);
  return 0;
}

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

int cmd_cp(const char *fsname, const char *filename1, const char *filename2, bool mode1, bool mode2)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  if (mode1 && mode2) // Si les deux fichiers sont internes
  {
    struct inode *in = find_inode(map, nb1, nbi, filename1);
    if (!in)
    {
      print_error("Fichier source introuvable, impossible à copier");
      close_fs(fd, map, size);
      return 1;
    }
    struct inode *in2 = find_inode(map, nb1, nbi, filename2);
    if (!in2)
    {
      int status = create_file(map, filename2);
      if (status == -1)
      {
        close_fs(fd, map, size);
        return 1;
      }
      in2 = find_inode(map, nb1, nbi, filename2);
    }
    else
    {
      dealloc_data_block(in2, map);
    }
    for (int i = 0; i < 900; i++)
    {
      int32_t b = FROM_LE32(in->direct_blocks[i]);
      if (b < 0)
        break;
      struct data_block *data_position = get_data_block(map, b);
      int32_t new_block = alloc_data_block(map);
      struct data_block *new_data_position = get_data_block(map, new_block);
      memcpy(new_data_position->data, data_position->data, 4000);
      in2->direct_blocks[i] = TO_LE32(new_block);
      in2->file_size = TO_LE32(FROM_LE32(in2->file_size) + 4000);
      in2->modification_time = TO_LE32(time(NULL));
    }
    if ((int32_t)FROM_LE32(in->double_indirect_block) >= 0)
    {
      struct address_block *dbl = get_address_block(map, FROM_LE32(in->double_indirect_block));
      if (FROM_LE32(dbl->type) == 6)
      {
        for (int i = 0; i < 1000; i++)
        {
          int32_t db = FROM_LE32(dbl->addresses[i]);
          if (db < 0)
            break;
          struct data_block *data_position2 = get_data_block(map, db);
          int32_t new_block = alloc_data_block(map);
          struct data_block *new_data_position2 = get_data_block(map, new_block);
          memcpy(new_data_position2->data, data_position2->data, 4000);
          in2->direct_blocks[i] = TO_LE32(new_block);
          in2->file_size = TO_LE32(FROM_LE32(in2->file_size) + 4000);
          in2->modification_time = TO_LE32(time(NULL));
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
            struct data_block *data_position3 = get_data_block(map, db2);
            int32_t new_block = alloc_data_block(map);
            struct data_block *new_data_position3 = get_data_block(map, new_block);
            memcpy(new_data_position3->data, data_position3->data, 4000);
            in2->direct_blocks[i] = TO_LE32(new_block);
            in2->file_size = TO_LE32(FROM_LE32(in2->file_size) + 4000);
            in2->modification_time = TO_LE32(time(NULL));
          }
        }
      }
    }
  }
  else if (mode1 && !mode2) // Si le premier fichier est interne et le second externe
  {
    struct inode *in = find_inode(map, nb1, nbi, filename1);
    if (!in)
    {
      print_error("Fichier source introuvable, impossible à copier");
      close_fs(fd, map, size);
      return 1;
    }
    int32_t file_size = FROM_LE32(in->file_size);
    int fd2 = open(filename2, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd2 < 0)
    {
      print_error("open: erreur d'ouverture du fichier externe");
      close_fs(fd, map, size);
      return 1;
    }
    for (int i = 0; i < 900 && file_size > 0; i++)
    {
      int32_t b = FROM_LE32(in->direct_blocks[i]);
      if (b < 0)
        break;
      uint8_t *data_position = (uint8_t *)get_data_block(map, b);
      uint32_t buffer = file_size < 4000 ? file_size : 4000;
      int written = write(fd2, data_position, buffer);
      if (written == -1)
      {
        print_error("write: erreur d'écriture");
        close_fs(fd, map, size);
        close(fd2);
        return 1;
      }
      file_size -= buffer;
    }
    if ((int32_t)FROM_LE32(in->double_indirect_block) < 0)
    {
      struct address_block *dbl = get_address_block(map, FROM_LE32(in->double_indirect_block));
      if (FROM_LE32(dbl->type) == 6)
      {
        for (int i = 0; i < 1000 && file_size > 0; i++)
        {
          int32_t db = FROM_LE32(dbl->addresses[i]);
          if (db < 0)
            break;
          uint8_t *data_position2 = (uint8_t *)get_data_block(map, db);
          uint32_t buffer = file_size < 4000 ? file_size : 4000;
          int written = write(fd2, data_position2, buffer);
          if (written == -1)
          {
            print_error("write: erreur d'écriture");
            close_fs(fd, map, size);
            close(fd2);
            return 1;
          }
          file_size -= buffer;
        }
      }
      else
      {
        for (int i = 0; i < 1000 && file_size > 0; i++)
        {
          int32_t db = FROM_LE32(dbl->addresses[i]);
          if (db < 0)
            break;
          struct address_block *sib = get_address_block(map, db);
          for (int j = 0; j < 1000 && file_size > 0; j++)
          {
            int32_t db2 = FROM_LE32(sib->addresses[j]);
            if (db2 < 0)
              break;
            struct data_block *data_position3 = get_data_block(map, db2);
            uint32_t buffer = file_size < 4000 ? file_size : 4000;
            int written = write(fd2, data_position3->data, buffer);
            if (written == -1)
            {
              print_error("write: erreur d'écriture");
              close_fs(fd, map, size);
              close(fd2);
              return 1;
            }
            file_size -= buffer;
          }
        }
      }
    }
    close(fd2);
  }
  else // Si le premier fichier est externe et le second interne
  {
    int fd2 = open(filename1, O_RDONLY);
    if (fd2 < 0)
    {
      print_error("open: erreur d'ouverture du fichier externe");
      close_fs(fd, map, size);
      return 1;
    }
    struct inode *in = find_inode(map, nb1, nbi, filename2);
    if (!in)
    {
      int status = create_file(map, filename2);
      if (status == -1)
      {
        close_fs(fd, map, size);
        close(fd2);
        return 1;
      }
      in = find_inode(map, nb1, nbi, filename2);
    }
    else
    {
      dealloc_data_block(in, map);
    }
    uint32_t total = 0;
    char buf[4000];
    size_t r;
    while ((r = read(fd2, buf, 4000)) > 0)
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
    in->file_size = TO_LE32(total);
    in->modification_time = TO_LE32(time(NULL));
    calcul_sha1(in, 4000, in->sha1);
    close(fd2);
  }
  close_fs(fd, map, size);
  return 0;
}

int cmd_rm(const char *fsname, const char *filename)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct inode *in = find_inode(map, nb1, nbi, filename);
  if (!in || !(FROM_LE32(in->flags) & 1))
  {
    print_error("Fichier introuvable");
    close_fs(fd, map, size);
    return 1;
  }

  delete_inode(in, map);

  close_fs(fd, map, size);
  return 0;
}

int cmd_lock(const char *fsname, const char *filename, const char *arg)
{
  signal(SIGTERM, handle_sigterm);
  signal(SIGINT, handle_sigint);

  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct inode *in = find_inode(map, nb1, nbi, filename);
  if (!in)
  {
    print_error("Fichier introuvable");
    close_fs(fd, map, size);
    return 1;
  }

  if (strcmp(arg, "r") == 0)
  {
    in->flags |= (1 << 3);
    while (keep_running)
    {
      sleep(1);
    }
    in->flags &= ~(1 << 3);
    printf("SIGTERM reçu. Fin du programme.\n");
  }
  else if (strcmp(arg, "w") == 0)
  {
    in->flags |= (1 << 4);
    while (keep_running)
    {
      sleep(1);
    }
    in->flags &= ~(1 << 4);
    printf("SIGTERM reçu. Fin du programme.\n");
  }

  return 0;
}

int cmd_chmod(const char *fsname, const char *filename, const char *arg)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct inode *in = find_inode(map, nb1, nbi, filename);
  if (!in)
  {
    print_error("Fichier introuvable");
    close_fs(fd, map, size);
    return 1;
  }
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
  return 0;
}

int cmd_cat(const char *fsname, const char *filename)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct inode *in = find_inode(map, nb1, nbi, filename);
  if (!in || !(FROM_LE32(in->flags) & 1))
  {
    print_error("Fichier non trouvé");
    close_fs(fd, map, size);
    return 1;
  }

  uint32_t file_size = FROM_LE32(in->file_size);
  uint32_t remaining_data = file_size;

  // 1) Direct blocs
  for (int i = 0; i < 900 && remaining_data > 0; i++)
  {
    int32_t b = FROM_LE32(in->direct_blocks[i]);
    if (b < 0)
      break;
    struct data_block *data_position = get_data_block(map, b);
    uint32_t buffer = remaining_data < 4000 ? remaining_data : 4000;
    int written = write(STDOUT_FILENO, data_position->data, buffer);
    if (written == -1)
    {
      print_error("write: erreur d'écriture");
      close_fs(fd, map, size);
      return 1;
    }
    remaining_data -= buffer;
  }

  if (remaining_data == 0)
  {
    goto end;
  }

  // 2) Simple/Doucle indirect blocs
  int32_t dib = FROM_LE32(in->double_indirect_block);
  if (dib < 0)
  {
    goto end;
  }
  struct address_block *dbl = get_address_block(map, dib);
  if (FROM_LE32(dbl->type) == 6)
  {
    for (int i = 0; i < 1000 && remaining_data > 0; i++)
    {
      int32_t db = FROM_LE32(dbl->addresses[i]);
      if (db < 0)
        break;
      struct data_block *data_position2 = get_data_block(map, db);
      uint32_t buffer = remaining_data < 4000 ? remaining_data : 4000;
      int written = write(STDOUT_FILENO, data_position2->data, buffer);
      if (written == -1)
      {
        print_error("write: erreur d'écriture");
        close_fs(fd, map, size);
        return 1;
      }
      remaining_data -= buffer;
    }
    goto end;
  }
  else
  {
    for (int i = 0; i < 1000 && remaining_data > 0; i++)
    {
      int32_t db = FROM_LE32(dbl->addresses[i]);
      if (db < 0)
        break;
      struct address_block *ab2 = get_address_block(map, db);
      for (int j = 0; j < 1000 && remaining_data > 0; j++)
      {
        int32_t db2 = FROM_LE32(ab2->addresses[j]);
        if (db2 < 0)
          break;
        struct data_block *data_position3 = get_data_block(map, db2);
        uint32_t buffer = remaining_data < 4000 ? remaining_data : 4000;
        int written = write(STDOUT_FILENO, data_position3->data, buffer);
        if (written == -1)
        {
          print_error("write: erreur d'écriture");
          close_fs(fd, map, size);
          return 1;
        }
        remaining_data -= buffer;
      }
    }
  }
end:
  close_fs(fd, map, size);
  return 0;
}

int cmd_input(const char *fsname, const char *filename)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct inode *in = find_inode(map, nb1, nbi, filename);
  if (!in)
  {
    int status = create_file(map, filename);
    if (status == -1)
    {
      return print_error("Erreur lors de la création du fichier");
    }
    in = find_inode(map, nb1, nbi, filename);
  }
  else
  {
    dealloc_data_block(in, map);
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
      memcpy(db->data + offset, buf, r);
      calcul_sha1(db->data, 4000, db->sha1);
      total += r;
      in->file_size = TO_LE32(total);
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
      memset(db->data, 0, sizeof(db->data));
      memcpy(db->data, buf, r);
      calcul_sha1(db->data, 4000, db->sha1);
      db->type = TO_LE32(5);
      total += r;
      in->file_size = TO_LE32(total);
    }
  }
  in->file_size = TO_LE32(total);
  in->modification_time = TO_LE32(time(NULL));
  calcul_sha1(in, 4000, in->sha1);
  close_fs(fd, map, size);
  return 0;
}

int cmd_add(const char *fsname, const char *filename_ext, const char *filename_int)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  int inf = open(filename_ext, O_RDONLY);

  if (inf < 0)
  {
    print_error("open source: erreur d'ouverture du fichier externe");
    return 1;
  }

  struct inode *in = find_inode(map, nb1, nbi, filename_int);
  if (!in)
  {
    int status = create_file(map, filename_int);
    if (status == -1)
    {
      return print_error("Erreur lors de la création du fichier interne");
    }
    in = find_inode(map, nb1, nbi, filename_int);
  }
  uint32_t total = FROM_LE32(in->file_size);
  uint32_t offset = (total % 4000);
  char buf[4000];
  size_t r;
  if (offset > 0)
  {
    int32_t last = get_last_data_block(map, in);
    r = read(inf, buf, 4000 - offset);
    if ((int32_t)r == -1)
    {
      print_error("read source: erreur de lecture du fichier externe");
      close_fs(fd, map, size);
      return 1;
    }
    if (r == 0)
    {
      close_fs(fd, map, size);
      return 0;
    }
    struct data_block *db = get_data_block(map, last);
    memcpy(db->data + offset, buf, r);
    calcul_sha1(db->data, 4000, db->sha1);
    total += r;
    in->file_size = TO_LE32(total);
  }

  while ((r = read(inf, buf, 4000)) > 0)
  {
    int32_t b = get_last_data_block_null(map, in);
    // printf("b = %d\n", b);
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

  // recalculer le SHA1 de des blocs d'adressage
  if ((int32_t)FROM_LE32(in->double_indirect_block) >= 0)
  {
    struct address_block *dbl = get_address_block(map, FROM_LE32(in->double_indirect_block));
    if (FROM_LE32(dbl->type) == 6)
    {
      calcul_sha1(dbl->addresses, 4000, dbl->sha1);
    }
    else
    {
      for (int i = 0; i < 1000; i++)
      {
        int32_t db = FROM_LE32(dbl->addresses[i]);
        if (db < 0)
          break;
        struct address_block *sib = get_address_block(map, db);
        calcul_sha1(sib->addresses, 4000, sib->sha1);
      }
      calcul_sha1(dbl->addresses, 4000, dbl->sha1);
    }
  }

  in->file_size = TO_LE32(total);
  in->modification_time = TO_LE32(time(NULL));
  calcul_sha1(in, 4000, in->sha1);
  close_fs(fd, map, size);
  close(inf);

  return 0;
}

int cmd_addinput(const char *fsname, const char *filename)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct inode *in = find_inode(map, nb1, nbi, filename);
  if (!in)
  {
    int status = create_file(map, filename);
    if (status == -1)
    {
      return print_error("Erreur lors de la création du fichier");
    }
    in = find_inode(map, nb1, nbi, filename);
  }
  uint32_t total = FROM_LE32(in->file_size);
  char buf[4000];
  size_t r;
  while ((r = read(STDIN_FILENO, buf, 4000)) > 0)
  {
    uint32_t offset = (total % 4000);
    if (offset > 0)
    {
      int32_t last = get_last_data_block(map, in);
      struct data_block *db = get_data_block(map, last);
      memcpy(db->data + offset, buf, r);
      calcul_sha1(db->data, 4000, db->sha1);
      total += r;
      in->file_size = TO_LE32(total);
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
      memset(db->data, 0, sizeof(db->data));
      memcpy(db->data, buf, r);
      calcul_sha1(db->data, 4000, db->sha1);
      db->type = TO_LE32(5);
      total += r;
      in->file_size = TO_LE32(total);
    }
  }
  // recalculer le SHA1 de des blocs d'adressage
  if ((int32_t)FROM_LE32(in->double_indirect_block) >= 0)
  {
    struct address_block *dbl = get_address_block(map, FROM_LE32(in->double_indirect_block));
    if (FROM_LE32(dbl->type) == 6)
    {
      calcul_sha1(dbl->addresses, 4000, dbl->sha1);
    }
    else
    {
      for (int i = 0; i < 1000; i++)
      {
        int32_t db = FROM_LE32(dbl->addresses[i]);
        if (db < 0)
          break;
        struct address_block *sib = get_address_block(map, db);
        calcul_sha1(sib->addresses, 4000, sib->sha1);
      }
      calcul_sha1(dbl->addresses, 4000, dbl->sha1);
    }
  }
  in->file_size = TO_LE32(total);
  in->modification_time = TO_LE32(time(NULL));
  calcul_sha1(in, 4000, in->sha1);
  close_fs(fd, map, size);
  return 0;
}

int cmd_fsck(const char *fsname)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct pignoufs *sb = (struct pignoufs *)map;

  if (strncmp(sb->magic, "pignoufs", 8) != 0)
  {
    print_error("Erreur: le conteneur n'est pas un système de fichiers Pignoufs");
    close_fs(fd, map, size);
    return 1;
  }

  for (int32_t i = 1; i < nbb; i++)
  {
    struct bitmap_block *bb = (struct bitmap_block *)((uint8_t *)map + (int64_t)(i) * 4096);
    if (i <= nb1)
    {
      if (bb->type != TO_LE32(2))
      {
        print_error("Erreur: bloc de bitmap a un type incorrect");
        close_fs(fd, map, size);
        return 1;
      }
    }
    else if (i <= nb1 + nbi)
    {
      if (bb->type != TO_LE32(3))
      {
        print_error("Erreur: bloc d'inode a un type incorrect");
        close_fs(fd, map, size);
        return 1;
      }
      struct inode *in = (struct inode *)bb;
      if (in->flags & 1) // Si l'inode est allouée
      {
        if (!check_bitmap_bit(map, i))
        {
          print_error("Erreur: inode a un bloc de données non alloué");
          close_fs(fd, map, size);
          return 1;
        }
        for (int j = 0; j < 900; j++)
        {
          int32_t b = FROM_LE32(in->direct_blocks[j]);
          if (b < 0)
            break;
          if (!check_bitmap_bit(map, b))
          {
            print_error("Erreur: inode a un bloc de données non alloué");
            close_fs(fd, map, size);
            return 1;
          }
        }
        if ((int32_t)FROM_LE32(in->double_indirect_block) < 0)
        {
          struct address_block *dbl = get_address_block(map, FROM_LE32(in->double_indirect_block));
          if (FROM_LE32(dbl->type) == 6)
          {
            for (int j = 0; j < 1000; j++)
            {
              int32_t db = FROM_LE32(dbl->addresses[j]);
              if (db < 0)
                break;
              if (!check_bitmap_bit(map, db))
              {
                print_error("Erreur: inode a un bloc de données indirect non alloué");
                close_fs(fd, map, size);
                return 1;
              }
            }
          }
          else
          {
            for (int j = 0; j < 1000; j++)
            {
              int32_t db = FROM_LE32(dbl->addresses[j]);
              if (db < 0)
                break;
              struct address_block *sib = get_address_block(map, db);
              for (int k = 0; k < 1000; k++)
              {
                int32_t db2 = FROM_LE32(sib->addresses[k]);
                if (db2 < 0)
                  break;
                if (!check_bitmap_bit(map, db2))
                {
                  print_error("Erreur: inode a un bloc de données indirect non alloué");
                  close_fs(fd, map, size);
                  return 1;
                }
              }
            }
          }
        }
      }
    }
    else
    {
      if (bb->type < TO_LE32(4) || bb->type > TO_LE32(7))
      {
        print_error("Erreur: bloc de données a un type incorrect");
        close_fs(fd, map, size);
        return 1;
      }
    }
    uint8_t sha1[20];
    calcul_sha1(bb->bits, 4000, sha1);
    if (memcmp(bb->sha1, sha1, 20) != 0)
    {
      print_error("Erreur: bloc de bitmap a un SHA1 incorrect");
      close_fs(fd, map, size);
      return 1;
    }
  }
  close_fs(fd, map, size);
  return 0;
}

int cmd_mount(int argc, char *argv[])
{
  printf("Exécution de la commande mount\n");
  return 0;
}