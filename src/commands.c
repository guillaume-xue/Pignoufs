#include "../include/structures.h"
#include "../include/utilitaires.h"

// Fonction de création du système de fichiers (mkfs)
void cmd_mkfs(int argc, char *argv[])
{
  const char *fsname = argv[1];
  if (argc < 4)
  {
    fprintf(stderr, "Usage: mkfs <fsname> <nbi> <nba>\n");
    return;
  }
  char *endptr;
  int32_t nbi = strtol(argv[2], &endptr, 10);
  if (*endptr != '\0')
  {
    fprintf(stderr, "Erreur: %s n'est pas un nombre valide.\n", argv[2]);
    return;
  }

  int32_t nba = strtol(argv[3], &endptr, 10);
  if (*endptr != '\0')
  {
    fprintf(stderr, "Erreur: %s n'est pas un nombre valide.\n", argv[3]);
    return;
  }

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
    perror("open");
    return EXIT_FAILURE;
  }
  uint64_t filesize = (uint64_t)nbb * 4096;

  if (ftruncate(fd, filesize) < 0)
  {
    perror("ftruncate");
    return EXIT_FAILURE;
  }

  // Projection de tout le conteneur en mémoire
  void *map = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED)
  {
    perror("mmap");
    return EXIT_FAILURE;
  }

  // 1) Superbloc
  struct pignoufs *sb = (struct pignoufs *)map;
  memset(sb, 0, sizeof(*sb));
  memcpy(sb->magic, "pignoufs", 8);
  sb->nb_b = htole32(nbb);
  sb->nb_i = htole32(nbi);
  sb->nb_a = htole32(nba);
  sb->nb_l = htole32(nba);
  sb->nb_f = htole32(0);
  // SHA1 + type
  uint8_t *sha1 = (uint8_t *)map;
  calcul_sha1(sha1, 4000, sha1 + 4000);
  struct bitmap_block *bb = (struct bitmap_block *)(sha1 + 4000);
  bb->type = htole32(1);

  // 2) Blocs de bitmap
  for (int32_t i = 0; i < nb1; i++)
  {
    struct bitmap_block *bb = (struct bitmap_block *)((uint8_t *)map + (uint64_t)(i + 1) * 4096);
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
    bb->type = htole32(2);
  }

  // 3) Blocs d'inodes
  for (int32_t i = 0; i < nbi; i++)
  {
    struct inode *in = (struct inode *)((uint8_t *)map + (uint64_t)(1 + nb1 + i) * 4096);
    memset(in, 0, sizeof(*in)); // marquer l'inode comme libre
    calcul_sha1(in, 4000, in->sha1);
    in->type = htole32(3);
  }

  // 4) Blocs de données
  for (int32_t i = 0; i < nba; i++)
  {
    struct data_block *db = (struct data_block *)((uint8_t *)map + (uint64_t)(1 + nb1 + nbi + i) * 4096);
    memset(db->data, 0, sizeof(db->data));
    calcul_sha1(db->data, 4000, db->sha1);
    db->type = htole32(4);
  }

  msync(map, filesize, MS_SYNC);
  munmap(map, filesize);
  close(fd);

  printf("Système de fichiers %s créé avec %d inodes et %d blocs allouables.\n", fsname, nbi, nba);
  printf("Nombre total de blocs : %d\n", nbb);
}

void cmd_ls(int argc, char *argv[])
{
  const char *fsname = argv[1];
  const char *filename = NULL;
  if (argc > 2)
  {
    filename = argv[2];
    if (strncmp(filename, "//", 2) != 0)
    {
      fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
      return;
    }
    filename += 2;
  }

  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return -1;

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct pignoufs *sb = (struct pignoufs *)map;
  uint32_t nbl = le32toh(sb->nb_l), nbf = le32toh(sb->nb_f);
  int32_t base = 1 + nb1;
  int found = 0;
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = (struct inode *)(map + (uint64_t)(base + i) * 4096);
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
          // printf("here\n");
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
    printf("%s introuvable\n", filename);
    close_fs(fd, map, size);
    return -1;
  }
  close_fs(fd, map, size);
  return 0;
}

void cmd_df(int argc, char *argv[])
{
  const char *fsname = argv[1];
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return -1;

  struct pignoufs *sb = (struct pignoufs *)map;
  printf("%d blocs de 4Kio libres\n", sb->nb_l);

  close_fs(fd, map, size);
  return 0;
}

void cmd_cp(int argc, char *argv[])
{
  printf("Exécution de la commande cp\n");
}

void cmd_rm(int argc, char *argv[])
{
  printf("Exécution de la commande rm\n");
}

void cmd_lock(int argc, char *argv[])
{
  printf("Exécution de la commande lock\n");
}

void cmd_chmod(int argc, char *argv[])
{
  printf("Exécution de la commande chmod\n");
}

void cmd_cat(int argc, char *argv[])
{
  const char *fsname = argv[1];
  const char *filename = argv[2];
  if (strncmp(filename, "//", 2) != 0)
  {
    fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
    return;
  }
  filename += 2;

  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return -1;

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct inode *in = find_inode(map, nb1, nbi, filename);
  if (!in || !(le32toh(in->flags) & 1))
  {
    printf("%s non trouvé\n", filename);
    close_fs(fd, map, size);
    return;
  }

  uint32_t file_size = le32toh(in->file_size);
  uint32_t remaining_data = file_size;

  // 1) Direct blocs
  for (int i = 0; i < 900 && remaining_data > 0; i++)
  {
    uint64_t b = le32toh(in->direct_blocks[i]);
    if (b < 0)
      break;
    uint64_t *data_position = map + (uint64_t)b * 4096;
    uint64_t buffer = remaining_data < 4000 ? remaining_data : 4000;
    write(STDOUT_FILENO, data_position, buffer);
    remaining_data -= buffer;
  }

  if (remaining_data == 0)
  {
    goto end;
  }

  // 2) Simple/Doucle indirect blocs
  int32_t dib = le32toh(in->double_indirect_block);
  if (dib < 0)
  {
    goto end;
  }
  printf("dib = %d\n", dib);
  struct address_block *dbl = (struct address_block *)(map + (uint64_t)dib * 4096);
  printf("dbl = %p\n", dbl);
  printf("dbl->type = %d\n", le32toh(dbl->type));
  if(le32toh(dbl->type) == 6){
    for (int i = 0; i < 1000 && remaining_data > 0; i++)
    {
      int32_t db = le32toh(dbl->addresses[i]);
      if (db < 0)
        break;
      uint64_t *data_position2 = map + (uint64_t)db * 4096;
      uint32_t buffer = remaining_data < 4000 ? remaining_data : 4000;
      write(STDOUT_FILENO, data_position2, buffer);
      remaining_data -= buffer;
    }
    goto end;
  }else{
    for (int i = 0; i < 1000 && remaining_data > 0; i++)
    {
      uint32_t db = le32toh(dbl->addresses[i]);
      if (db < 0)
        break;
      uint64_t *data_position2 = map + (uint64_t)db * 4096;
      struct address_block *ab2 = (struct address_block *)data_position2;
      for (int j = 0; j < 1000 && remaining_data > 0; j++)
      {
        uint32_t db2 = le32toh(ab2->addresses[j]);
        if (db2 < 0)
          break;
        uint64_t *data_position3 = map + (uint64_t)db2 * 4096;
        uint32_t buffer = remaining_data < 4000 ? remaining_data : 4000;
        write(STDOUT_FILENO, data_position3, buffer);
        remaining_data -= buffer;
      }
    }
    end:
    close_fs(fd, map, size);
    return;
  }
  close_fs(fd, map, size);
}

void cmd_input(int argc, char *argv[])
{
  const char *fsname = argv[1];
  const char *filename = argv[2];
  if (strncmp(filename, "//", 2) != 0)
  {
    fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
    return;
  }
  filename += 2;

  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return -1;

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct inode *in = find_inode(map, nb1, nbi, filename);
  if (!in)
  {
    int status = create_file(map, filename);
    if (status == -1)
    {
      return -1;
    }
    in = find_inode(map, nb1, nbi, filename);
  }else{
    dealloc_data_block(in, map);
  }

  struct pignoufs *sb = (struct pignoufs *)map;

  char buf[4000]; 
  size_t r;
  uint32_t total = in->file_size;
  while ((r = read(STDIN_FILENO, buf, 4000)) > 0)
  {
    uint32_t offset = (in->file_size % 4000);
    if (offset > 0) {
      int32_t last = get_last_data_block(map,in,nb1,nbi,nba,nbb);
      struct data_block *db = (struct data_block *)(map + (uint64_t)last * 4096);
      memcpy(db->data+offset, buf, r);
      calcul_sha1(db->data, 4000, db->sha1);
      total += r;
      in->file_size = total;
    }else{
      uint32_t b = get_last_data_block_null(map,in,nb1,nbi,nba,nbb);
      if (b == -2) {
        perror("Erreur: pas de blocs de données libres disponibles\n");
        break;
      }
      struct data_block *db = (struct data_block *)(map + (uint64_t)b * 4096);
  
      memcpy(db->data, buf, r);
      calcul_sha1(db->data, 4000, db->sha1);
      db->type = 5;
      total += r;
      in->file_size = total;
    }
  }
  in->file_size = htole32(total);
  in->modification_time = htole32(time(NULL));
  calcul_sha1(in, 4000, in->sha1);
  close_fs(fd, map, size);
  return 0;
}

int cmd_add(int argc, char *argv[])
{
  const char *fsname = argv[1];
  const char *filename_ext = argv[2];
  const char *filename_int = argv[3];

  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return -1;

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct pignoufs *sb = (struct pignoufs *)map;
  int inf = open(filename_ext, O_RDONLY);

  if (inf < 0)
  {
    perror("open source");
    return;
  }

  struct inode *in = find_inode(map, nb1, nbi, filename_int);
  if (!in)
  {
    int status = create_file(map, filename_int);
    if (status == -1)
    {
      return -1;
    }
    in = find_inode(map, nb1, nbi, filename_int);
  }
  uint32_t offset = (in->file_size % 4000);
  char buf[4000]; size_t r; uint32_t total = in->file_size;
  if (offset > 0) {
    int32_t last = get_last_data_block(map,in,nb1,nbi,nba,nbb);
    r = read(inf, buf, 4000-offset);
    struct data_block *db = (struct data_block *)(map + (uint64_t)last * 4096);
    memcpy(db->data+offset, buf, r);
    calcul_sha1(db->data, 4000, db->sha1);
    total += r;
    in->file_size = total;
    
  }

  while ((r = read(inf, buf, 4000)) > 0) {
    uint32_t b = get_last_data_block_null(map,in,nb1,nbi,nba,nbb);
    if (b == -2) {
      perror("Erreur: pas de blocs de données libres disponibles\n");
      break;
    }
    
    struct data_block *db = (struct data_block *)(map + (uint64_t)b * 4096);
    memset(db->data, 0, sizeof(db->data));
    memcpy(db->data, buf, r);
    calcul_sha1(db->data, 4000, db->sha1);
    db->type = 5;
    total += r;
    in->file_size = total;
  }
  in->file_size = total;
  in->modification_time = htole32(time(NULL));
  calcul_sha1(in, 4000, in->sha1);
  close_fs(fd, map, size);
  close(inf);

  return 0;
}

void cmd_addinput(int argc, char *argv[])
{
  printf("Exécution de la commande addinput\n");
}

void cmd_fsck(int argc, char *argv[])
{
  printf("Exécution de la commande fsck\n");
}

void cmd_mount(int argc, char *argv[])
{
  printf("Exécution de la commande mount\n");
}