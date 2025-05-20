#include "../include/structures.h"
#include "../include/utilitaires.h"

#define NB_THREADS 6

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

  for (int32_t i = 0; i < nbb; i++)
  {
    calcul_sha1(map + (int64_t)i * 4096, 4000, ((uint8_t *)map + (int64_t)i * 4096) + 4000);
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
    struct inode *in = get_inode(map, 1 + nb1 + i);
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

// Affichage récursif de l'arborescence
void print_tree(uint8_t *map, int32_t nb1, int32_t nbi, struct inode *inode, int depth, bool *visited_inodes)
{
  // Vérifier si l'inode a déjà été visité pour éviter les boucles infinies
  int inode_index = inode - get_inode(map, 1 + nb1);
  if (visited_inodes[inode_index])
    return;
  visited_inodes[inode_index] = true;

  // Afficher l'indentation en fonction de la profondeur
  for (int i = 0; i < depth; i++)
    printf("  ");

  // Afficher le nom du fichier ou répertoire
  printf("%s%s\n", inode->filename, ((FROM_LE32(inode->flags) >> 5) & 1) ? "/" : "");

  // Si ce n'est pas un répertoire, arrêter
  if (!((FROM_LE32(inode->flags) >> 5) & 1))
    return;

  // Si c'est une racine, afficher l'arborescence complète
  for (int i = 0; i < 900; i++)
  {
    int32_t block_idx = FROM_LE32(inode->direct_blocks[i]);
    if (block_idx == -1)
      continue;
    
    struct inode *child_inode = get_inode(map, block_idx);
    print_tree(map, nb1, nbi, child_inode, depth + 1, visited_inodes);
  }
}

// Commande tree
int cmd_tree(const char *fsname)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  bool *visited_inodes = calloc(nbi, sizeof(bool));
  if (!visited_inodes)
  {
    close_fs(fd, map, size);
    return print_error("Erreur d'allocation mémoire");
  }

  int racine_trouvee = 0;
  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = get_inode(map, 1 + nb1 + i);
    
    if ((FROM_LE32(in->flags) & 1) && ((FROM_LE32(in->flags) >> 5) & 1) && FROM_LE32(in->profondeur) == 0)
    {
      print_tree(map, nb1, nbi, in, 0, visited_inodes);
      racine_trouvee = 1;
    }
  }

  if (!racine_trouvee)
  {
    free(visited_inodes);
    close_fs(fd, map, size);
    return print_error("Aucune racine trouvée");
  }

  free(visited_inodes);
  close_fs(fd, map, size);
  return 0;
}

int cmd_ls(const char *fsname, const char *filename, const char *argument)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");
  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  if(filename == NULL){ // racine print
    for(int i = 0; i < nbi; i++)
    {
      struct inode *in = get_inode(map, 1 + nb1 + i);
      if ((in->flags & 1) && FROM_LE32(in->profondeur) == 0)
      {
        if(argument != NULL)
        {
          if(strcmp(argument, "-l") == 0)
          {
            print_ls(in);
          }
        }
        else
        {
          printf("%s%s\n", in->filename, ((FROM_LE32(in->flags) >> 5) & 1) ? "/" : "");
        }
      }
    }
  }
  else{
    char parent_path[256], dir_name[256];
    split_path(filename, parent_path, dir_name);
    int val = find_inode_folder(map, nb1, nbi, parent_path);
    struct inode *in = get_inode(map, val);
    if(in == NULL){
      close_fs(fd, map, size);
      return print_error("Erreur: le répertoire n'existe pas");
    }
    struct inode *file = NULL;
    if(strlen(dir_name) == 0){
      file = in;
    }
    else{
      val = find_file_folder_from_inode(map, in, dir_name, false);
      if(val == -1){
        close_fs(fd, map, size);
        return print_error("Erreur: le fichier n'existe pas");
      }
      file = get_inode(map, val);
      if(file == NULL){
        close_fs(fd, map, size);
        return print_error("Erreur: le fichier n'existe pas");
      }
    }
    if((file->flags >> 5) & 1)
    {
      for(int i = 0; i < 900; i++)
      {
        if(file->direct_blocks[i] != -1)
        {
          struct inode *child_inode = get_inode(map, FROM_LE32(file->direct_blocks[i]));
          if (child_inode->flags & 1)
          {
            if(argument != NULL)
            {
              if (strcmp(argument, "-l") == 0)
              {
                print_ls(child_inode);
              }
            }
            else
            {
              printf("%s%s\n", child_inode->filename, ((FROM_LE32(child_inode->flags) >> 5) & 1) ? "/" : "");
            }
          }
        }
      }
    }
    else
    {
      if (argument != NULL)
      {
        if (strcmp(argument, "-l") == 0)
        {
          print_ls(file);
        }
      }
      else
      {
        printf("%s%s\n", file->filename, ((FROM_LE32(file->flags) >> 5) & 1) ? "/" : "");
      }
    }
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

int cmd_cp(const char *fsname, const char *filename1, const char *filename2, bool mode1, bool mode2, bool directory)
{
//   uint8_t *map;
//   size_t size;
//   int fd = open_fs(fsname, &map, &size);
//   if (fd < 0)
//     return print_error("Erreur lors de l'ouverture du système de fichiers");

//   int32_t nb1, nbi, nba, nbb;
//   get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

//   char parent_path[256], dir_name[256];
//   split_path(filename1, parent_path, dir_name);
    
//   char parent_path2[256], dir_name2[256];
//   split_path(filename2, parent_path2, dir_name2);

//   struct inode *in, *in2, *file1, *file2;


//   if (mode1 && mode2 && !directory) // Si les deux fichiers sont internes
//   {
//     if(strlen(parent_path) == 0){
//       file1 = find_inode_racine(map, nb1, nbi, filename1, false);
//     }
//     else{
//       in = find_inode_folder(map, nb1, nbi, parent_path);
//       file1 = find_file_folder_from_inode(map, in, dir_name, false);
//     }
//     if(strlen(parent_path2) == 0){
//       file2 = find_inode_racine(map, nb1, nbi, filename2, false);
//       if(file2){
//         dealloc_data_block(file2, map);
//         calcul_sha1(file2, 4000, file2->sha1);
//       }else{
//         int val = create_file(map, filename2);
//         if(val < 0){
//           close_fs(fd, map, size);
//           return val;
//         }
//       }
//     }
//     else{
//       in2 = find_inode_folder(map, nb1, nbi, parent_path2);
//       file2 = find_file_folder_from_inode(map, in2, dir_name2, false);
//       if(file2){
//         dealloc_data_block(file2, map);
//       }else{
//         int val = create_file(map, filename2);
//         if(val < 0){
//           close_fs(fd, map, size);
//           return val;
//         }
//         add_inode(in2, val);
//         calcul_sha1(in2, 4000, in2->sha1);
//         file2 = get_inode(map, 1 + nb1 + val);
//       }
//     }

//     for (int i = 0; i < 900; i++)
//     {
//       int32_t b = FROM_LE32(file1->direct_blocks[i]);
//       if (b < 0)
//         break;

//       // Verrouiller le bloc source
//       if (lock_block(fd, b * 4096, F_RDLCK) < 0)
//       {
//         print_error("Erreur lors du verrouillage du bloc source");
//         close_fs(fd, map, size);
//         return 1;
//       }

//       struct data_block *data_position = get_data_block(map, b);
//       int32_t new_block = alloc_data_block(map);

//       // Verrouiller le bloc destination
//       if (lock_block(fd, new_block * 4096, F_WRLCK) < 0)
//       {
//         print_error("Erreur lors du verrouillage du bloc destination");
//         unlock_block(fd, b * 4096);
//         close_fs(fd, map, size);
//         return 1;
//       }

//       struct data_block *new_data_position = get_data_block(map, new_block);
//       memcpy(new_data_position->data, data_position->data, 4000);
//       file2->direct_blocks[i] = TO_LE32(new_block);
//       file2->file_size = TO_LE32(FROM_LE32(file2->file_size) + 4000);
//       file2->modification_time = TO_LE32(time(NULL));
//       calcul_sha1(new_data_position->data, 4000, new_data_position->sha1);

//       // Déverrouiller les blocs
//       unlock_block(fd, b * 4096);
//       unlock_block(fd, new_block * 4096);
//     }
//     if ((int32_t)FROM_LE32(file1->double_indirect_block) >= 0)
//     {
//       struct address_block *dbl = get_address_block(map, FROM_LE32(file1->double_indirect_block));
//       if (FROM_LE32(dbl->type) == 6)
//       {
//         for (int i = 0; i < 1000; i++)
//         {
//           int32_t db = FROM_LE32(dbl->addresses[i]);
//           if (db < 0)
//             break;

//           // Verrouiller le bloc source
//           if (lock_block(fd, db * 4096, F_RDLCK) < 0)
//           {
//             print_error("Erreur lors du verrouillage du bloc source");
//             close_fs(fd, map, size);
//             return 1;
//           }

//           struct data_block *data_position2 = get_data_block(map, db);
//           int32_t new_block = alloc_data_block(map);

//           // Verrouiller le bloc destination
//           if (lock_block(fd, new_block * 4096, F_WRLCK) < 0)
//           {
//             print_error("Erreur lors du verrouillage du bloc destination");
//             unlock_block(fd, db * 4096);
//             close_fs(fd, map, size);
//             return 1;
//           }

//           struct data_block *new_data_position2 = get_data_block(map, new_block);
//           memcpy(new_data_position2->data, data_position2->data, 4000);
//           file2->direct_blocks[i] = TO_LE32(new_block);
//           file2->file_size = TO_LE32(FROM_LE32(file2->file_size) + 4000);
//           file2->modification_time = TO_LE32(time(NULL));
//           calcul_sha1(new_data_position2->data, 4000, new_data_position2->sha1);

//           // Déverrouiller les blocs
//           unlock_block(fd, db * 4096);
//           unlock_block(fd, new_block * 4096);
//         }
//       }
//       else
//       {
//         for (int i = 0; i < 1000; i++)
//         {
//           int32_t db = FROM_LE32(dbl->addresses[i]);
//           if (db < 0)
//             break;

//           // Verrouiller le bloc source
//           if (lock_block(fd, db * 4096, F_RDLCK) < 0)
//           {
//             print_error("Erreur lors du verrouillage du bloc source");
//             close_fs(fd, map, size);
//             return 1;
//           }

//           struct address_block *sib = get_address_block(map, db);
//           for (int j = 0; j < 1000; j++)
//           {
//             int32_t db2 = FROM_LE32(sib->addresses[j]);
//             if (db2 < 0)
//               break;

//             // Verrouiller le bloc source
//             if (lock_block(fd, db2 * 4096, F_RDLCK) < 0)
//             {
//               print_error("Erreur lors du verrouillage du bloc source");
//               unlock_block(fd, db * 4096);
//               close_fs(fd, map, size);
//               return 1;
//             }

//             struct data_block *data_position3 = get_data_block(map, db2);
//             int32_t new_block = alloc_data_block(map);

//             // Verrouiller le bloc destination
//             if (lock_block(fd, new_block * 4096, F_WRLCK) < 0)
//             {
//               print_error("Erreur lors du verrouillage du bloc destination");
//               unlock_block(fd, db2 * 4096);
//               unlock_block(fd, db * 4096);
//               close_fs(fd, map, size);
//               return 1;
//             }

//             struct data_block *new_data_position3 = get_data_block(map, new_block);
//             memcpy(new_data_position3->data, data_position3->data, 4000);
//             file2->direct_blocks[i] = TO_LE32(new_block);
//             file2->file_size = TO_LE32(FROM_LE32(file2->file_size) + 4000);
//             file2->modification_time = TO_LE32(time(NULL));
//             calcul_sha1(new_data_position3->data, 4000, new_data_position3->sha1);

//             // Déverrouiller les blocs
//             unlock_block(fd, db2 * 4096);
//             unlock_block(fd, new_block * 4096);
//           }

//           // Déverrouiller le bloc source
//           unlock_block(fd, db * 4096);
//         }
//       }
//     }
//     calcul_sha1(file2, 4000, file2->sha1);
//   }
//   else if (mode1 && !mode2 && !directory) // Si le premier fichier est interne et le second externe
//   {
//     if(strlen(parent_path) == 0){
//       file1 = find_inode_racine(map, nb1, nbi, filename1, false);
//     }
//     else{
//       in = find_inode_folder(map, nb1, nbi, parent_path);
//       file1 = find_file_folder_from_inode(map, in, dir_name, false);
//     }
    
//     int32_t file_size = FROM_LE32(file1->file_size);
//     int fd2 = open(filename2, O_WRONLY | O_CREAT | O_TRUNC, 0666);
//     if (fd2 < 0)
//     {
//       print_error("open: erreur d'ouverture du fichier externe");
//       close_fs(fd, map, size);
//       return 1;
//     }
//     for (int i = 0; i < 900 && file_size > 0; i++)
//     {
//       int32_t b = FROM_LE32(file1->direct_blocks[i]);
//       if (b < 0)
//         break;

//       // Verrouiller le bloc source
//       if (lock_block(fd, b * 4096, F_RDLCK) < 0)
//       {
//         print_error("Erreur lors du verrouillage du bloc source");
//         close_fs(fd, map, size);
//         close(fd2);
//         return 1;
//       }

//       uint8_t *data_position = (uint8_t *)get_data_block(map, b);
//       uint32_t buffer = file_size < 4000 ? file_size : 4000;
//       int written = write(fd2, data_position, buffer);
//       if (written == -1)
//       {
//         print_error("write: erreur d'écriture");
//         unlock_block(fd, b * 4096);
//         close_fs(fd, map, size);
//         close(fd2);
//         return 1;
//       }
//       file_size -= buffer;

//       // Déverrouiller le bloc source
//       unlock_block(fd, b * 4096);
//     }
//     if ((int32_t)FROM_LE32(file1->double_indirect_block) < 0)
//     {
//       struct address_block *dbl = get_address_block(map, FROM_LE32(file1->double_indirect_block));
//       if (FROM_LE32(dbl->type) == 6)
//       {
//         for (int i = 0; i < 1000 && file_size > 0; i++)
//         {
//           int32_t db = FROM_LE32(dbl->addresses[i]);
//           if (db < 0)
//             break;

//           // Verrouiller le bloc source
//           if (lock_block(fd, db * 4096, F_RDLCK) < 0)
//           {
//             print_error("Erreur lors du verrouillage du bloc source");
//             close_fs(fd, map, size);
//             close(fd2);
//             return 1;
//           }

//           uint8_t *data_position2 = (uint8_t *)get_data_block(map, db);
//           uint32_t buffer = file_size < 4000 ? file_size : 4000;
//           int written = write(fd2, data_position2, buffer);
//           if (written == -1)
//           {
//             print_error("write: erreur d'écriture");
//             unlock_block(fd, db * 4096);
//             close_fs(fd, map, size);
//             close(fd2);
//             return 1;
//           }
//           file_size -= buffer;

//           // Déverrouiller le bloc source
//           unlock_block(fd, db * 4096);
//         }
//       }
//       else
//       {
//         for (int i = 0; i < 1000 && file_size > 0; i++)
//         {
//           int32_t db = FROM_LE32(dbl->addresses[i]);
//           if (db < 0)
//             break;

//           // Verrouiller le bloc source
//           if (lock_block(fd, db * 4096, F_RDLCK) < 0)
//           {
//             print_error("Erreur lors du verrouillage du bloc source");
//             close_fs(fd, map, size);
//             close(fd2);
//             return 1;
//           }

//           struct address_block *sib = get_address_block(map, db);
//           for (int j = 0; j < 1000 && file_size > 0; j++)
//           {
//             int32_t db2 = FROM_LE32(sib->addresses[j]);
//             if (db2 < 0)
//               break;

//             // Verrouiller le bloc source
//             if (lock_block(fd, db2 * 4096, F_RDLCK) < 0)
//             {
//               print_error("Erreur lors du verrouillage du bloc source");
//               unlock_block(fd, db * 4096);
//               close_fs(fd, map, size);
//               close(fd2);
//               return 1;
//             }

//             struct data_block *data_position3 = get_data_block(map, db2);
//             uint32_t buffer = file_size < 4000 ? file_size : 4000;
//             int written = write(fd2, data_position3->data, buffer);
//             if (written == -1)
//             {
//               print_error("write: erreur d'écriture");
//               unlock_block(fd, db2 * 4096);
//               unlock_block(fd, db * 4096);
//               close_fs(fd, map, size);
//               close(fd2);
//               return 1;
//             }
//             file_size -= buffer;

//             // Déverrouiller le bloc source
//             unlock_block(fd, db2 * 4096);
//           }

//           // Déverrouiller le bloc source
//           unlock_block(fd, db * 4096);
//         }
//       }
//     }
//     close(fd2);
//   }
//   else // Si le premier fichier est externe et le second interne
//   {
//     int fd2 = open(filename1, O_RDONLY);
//     if (fd2 < 0)
//     {
//       print_error("open: erreur d'ouverture du fichier externe");
//       close_fs(fd, map, size);
//       return 1;
//     }

//     if(strlen(parent_path2) == 0){
//       file2 = find_inode_racine(map, nb1, nbi, filename2, false);
//       if(file2){
//         dealloc_data_block(file2, map);
//         calcul_sha1(file2, 4000, file2->sha1);
//       }else{
//         int val = create_file(map, filename2);
//         if(val < 0){
//           close_fs(fd, map, size);
//           return val;
//         }
//       }
//     }
//     else{
//       in2 = find_inode_folder(map, nb1, nbi, parent_path2);
//       file2 = find_file_folder_from_inode(map, in2, dir_name2, false);
//       if(file2){
//         dealloc_data_block(file2, map);
//       }else{
//         int val = create_file(map, filename2);
//         if(val < 0){
//           close_fs(fd, map, size);
//           return val;
//         }
//         add_inode(in2, val);
//         calcul_sha1(in2, 4000, in2->sha1);
//         file2 = get_inode(map, 1 + nb1 + val);
//       }
//     }

//     uint32_t total = 0;
//     char buf[4000];
//     size_t r;
//     while ((r = read(fd2, buf, 4000)) > 0)
//     {
//       int32_t b = get_last_data_block_null(map, file2);
//       if (b == -2)
//       {
//         print_error("Erreur: pas de blocs de données libres disponibles");
//         break;
//       }

//       // Verrouiller le bloc destination
//       if (lock_block(fd, b * 4096, F_WRLCK) < 0)
//       {
//         print_error("Erreur lors du verrouillage du bloc destination");
//         close_fs(fd, map, size);
//         close(fd2);
//         return 1;
//       }

//       struct data_block *db = get_data_block(map, b);
//       memset(db->data, 0, sizeof(db->data));
//       memcpy(db->data, buf, r);
//       calcul_sha1(db->data, 4000, db->sha1);
//       db->type = TO_LE32(5);
//       total += r;
//       file2->file_size = TO_LE32(total);

//       // Déverrouiller le bloc destination
//       unlock_block(fd, b * 4096);
//     }
//     file2->file_size = TO_LE32(total);
//     file2->modification_time = TO_LE32(time(NULL));
//     calcul_sha1(file2, 4000, file2->sha1);
//     close(fd2);
//   }
//   close_fs(fd, map, size);
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

  char parent_path[256], dir_name[256];
  split_path(filename, parent_path, dir_name);

  struct inode *in = NULL;
  int val = -1;

  if(strlen(parent_path) == 0){
    val = find_inode_racine(map, nb1, nbi, dir_name, false);
    if (val < 0){
      print_error("Fichier introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
  }
  else{
    val = find_inode_folder(map, nb1, nbi, parent_path);
    if (val < 0){
      print_error("Répertoire introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
    val = find_file_folder_from_inode(map, in, dir_name, false);
    if (val < 0){
      print_error("Fichier introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
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

  char parent_path[256], dir_name[256];
  split_path(filename, parent_path, dir_name);
  struct inode *in = NULL;
  int val = -1;
  bool is_dir = is_folder_path(parent_path, dir_name);

  if(is_dir){
    split_path(parent_path, parent_path, dir_name);
  }    
  if(strlen(parent_path) == 0){
    val = find_inode_racine(map, nb1, nbi, dir_name, false);
    if (val < 0){
      print_error("Fichier introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
  }
  else
  {
    val = find_inode_folder(map, nb1, nbi, parent_path);
    if (val < 0){
      print_error("Répertoire introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
    val = find_file_folder_from_inode(map, in, dir_name, false);
    if (val < 0){
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

int cmd_cat(const char *fsname, const char *filename)
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

  if(strlen(parent_path) == 0){
    val = find_inode_racine(map, nb1, nbi, dir_name, false);
    if (val < 0){
      print_error("Fichier introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
  }
  else{
    val = find_inode_folder(map, nb1, nbi, parent_path);
    if (val < 0){
      print_error("Répertoire introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
    val = find_file_folder_from_inode(map, in, dir_name, false);
    if (val < 0){
      print_error("Fichier introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
  }

  uint32_t file_size = FROM_LE32(in->file_size);
  uint32_t remaining_data = file_size;

  // 1) Direct blocs
  for (int i = 0; i < 900 && remaining_data > 0; i++)
  {
    int32_t b = FROM_LE32(in->direct_blocks[i]);
    if (b < 0)
      break;

    // Verrouiller le bloc source
    if (lock_block(fd, b * 4096, F_RDLCK) < 0)
    {
      print_error("Erreur lors du verrouillage du bloc source");
      close_fs(fd, map, size);
      return 1;
    }

    struct data_block *data_position = get_data_block(map, b);
    uint32_t buffer = remaining_data < 4000 ? remaining_data : 4000;
    int written = write(STDOUT_FILENO, data_position->data, buffer);
    if (written == -1)
    {
      print_error("write: erreur d'écriture");
      unlock_block(fd, b * 4096);
      close_fs(fd, map, size);
      return 1;
    }
    remaining_data -= buffer;

    // Déverrouiller le bloc source
    unlock_block(fd, b * 4096);
  }

  int32_t dib = FROM_LE32(in->double_indirect_block);
  if (remaining_data == 0 || dib < 0)
  {
    goto end;
  }

  // 2) Simple/Doucle indirect blocs
  struct address_block *dbl = get_address_block(map, dib);
  if (FROM_LE32(dbl->type) == 6)
  {
    for (int i = 0; i < 1000 && remaining_data > 0; i++)
    {
      int32_t db = FROM_LE32(dbl->addresses[i]);
      if (db < 0)
        continue; // Cas fichier à trou

      // Verrouiller le bloc source
      if (lock_block(fd, db * 4096, F_RDLCK) < 0)
      {
        print_error("Erreur lors du verrouillage du bloc source");
        close_fs(fd, map, size);
        return 1;
      }

      struct data_block *data_position2 = get_data_block(map, db);
      uint32_t buffer = remaining_data < 4000 ? remaining_data : 4000;
      int written = write(STDOUT_FILENO, data_position2->data, buffer);
      if (written == -1)
      {
        print_error("write: erreur d'écriture");
        unlock_block(fd, db * 4096);
        close_fs(fd, map, size);
        return 1;
      }
      remaining_data -= buffer;
      unlock_block(fd, db * 4096);
    }
  }
  else
  {
    for (int i = 0; i < 1000 && remaining_data > 0; i++)
    {
      int32_t db = FROM_LE32(dbl->addresses[i]);
      if (db < 0)
        continue; // Cas fichier à trou

      struct address_block *ab2 = get_address_block(map, db);
      for (int j = 0; j < 1000 && remaining_data > 0; j++)
      {
        int32_t db2 = FROM_LE32(ab2->addresses[j]);
        if (db2 < 0)
          continue; // Cas fichier à trou

        // Verrouiller le bloc source
        if (lock_block(fd, db2 * 4096, F_RDLCK) < 0)
        {
          print_error("Erreur lors du verrouillage du bloc source");
          close_fs(fd, map, size);
          return 1;
        }

        struct data_block *data_position3 = get_data_block(map, db2);
        uint32_t buffer = remaining_data < 4000 ? remaining_data : 4000;
        int written = write(STDOUT_FILENO, data_position3->data, buffer);
        if (written == -1)
        {
          print_error("write: erreur d'écriture");
          unlock_block(fd, db2 * 4096);
          close_fs(fd, map, size);
          return 1;
        }
        remaining_data -= buffer;
        unlock_block(fd, db2 * 4096);
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

  char parent_path[256], dir_name[256];
  split_path(filename, parent_path, dir_name);
  struct inode *in = NULL;
  int val = -1;

  if(strlen(parent_path) == 0){
    val = find_inode_racine(map, nb1, nbi, dir_name, false);
    if (val == -1)
    {
      val = create_file(map, dir_name);
    }
    in = get_inode(map, val);
  }
  else{
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
      }else{
        in = get_inode(map, val);
        dealloc_data_block(in, map);
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

  char parent_path[256], dir_name[256];
  split_path(filename_int, parent_path, dir_name);
  struct inode *in = NULL;
  int val = -1;

  if(strlen(parent_path) == 0){
    val = find_inode_racine(map, nb1, nbi, dir_name, false);
    if (val == -1)
    {
      val = create_file(map, dir_name);
    }
    in = get_inode(map, val);
  }
  else{
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
      }else{
        in = get_inode(map, val);
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

  uint32_t total = FROM_LE32(in->file_size);
  uint32_t offset = (total % 4000);
  char buf[4000];
  size_t r;

  if (offset > 0)
  {
    int32_t last = get_last_data_block(map, in);
    // Verrouiller le dernier bloc de données
    if (lock_block(fd, last * 4096, F_WRLCK) < 0)
    {
      print_error("Erreur lors du verrouillage du dernier bloc de données");
      unlock_block(fd, inode_offset);
      close_fs(fd, map, size);
      return 1;
    }

    r = read(inf, buf, 4000 - offset);
    if ((int32_t)r == -1)
    {
      print_error("read source: erreur de lecture du fichier externe");
      unlock_block(fd, last * 4096);
      unlock_block(fd, inode_offset);
      close_fs(fd, map, size);
      return 1;
    }
    if (r == 0)
    {
      unlock_block(fd, last * 4096);
      unlock_block(fd, inode_offset);
      close_fs(fd, map, size);
      return 0;
    }
    struct data_block *db = get_data_block(map, last);
    memcpy(db->data + offset, buf, r);
    calcul_sha1(db->data, 4000, db->sha1);
    total += r;
    in->file_size = TO_LE32(total);

    // Déverrouiller le dernier bloc de données
    unlock_block(fd, last * 4096);
  }

  while ((r = read(inf, buf, 4000)) > 0)
  {
    int32_t b = get_last_data_block_null(map, in);
    if (b == -2)
    {
      print_error("Erreur: pas de blocs de données libres disponibles");
      break;
    }

    // Verrouiller le nouveau bloc de données
    if (lock_block(fd, b * 4096, F_WRLCK) < 0)
    {
      print_error("Erreur lors du verrouillage du bloc de données");
      unlock_block(fd, inode_offset);
      close_fs(fd, map, size);
      return 1;
    }

    struct data_block *db = get_data_block(map, b);
    memset(db->data, 0, sizeof(db->data));
    memcpy(db->data, buf, r);
    calcul_sha1(db->data, 4000, db->sha1);
    db->type = TO_LE32(5);
    total += r;
    in->file_size = TO_LE32(total);

    // Déverrouiller le nouveau bloc de données
    unlock_block(fd, b * 4096);
  }

  // recalculer le SHA1 des blocs d'adressage
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

  // Déverrouiller l'inode
  unlock_block(fd, inode_offset);

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

  char parent_path[256], dir_name[256];
  split_path(filename, parent_path, dir_name);
  struct inode *in = NULL;
  int val = -1;

  if(strlen(parent_path) == 0){
    val = find_inode_racine(map, nb1, nbi, dir_name, false);
    if (val == -1)
    {
      val = create_file(map, dir_name);
    }
    in = get_inode(map, val);
  }
  else{
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
      }else{
        in = get_inode(map, val);
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

  uint32_t total = FROM_LE32(in->file_size);
  char buf[4000];
  size_t r;

  while ((r = read(STDIN_FILENO, buf, 4000)) > 0)
  {
    uint32_t offset = (total % 4000);
    if (offset > 0)
    {
      int32_t last = get_last_data_block(map, in);

      // Verrouiller le dernier bloc de données
      if (lock_block(fd, last * 4096, F_WRLCK) < 0)
      {
        print_error("Erreur lors du verrouillage du dernier bloc de données");
        unlock_block(fd, inode_offset);
        close_fs(fd, map, size);
        return 1;
      }

      struct data_block *db = get_data_block(map, last);
      memcpy(db->data + offset, buf, r);
      calcul_sha1(db->data, 4000, db->sha1);
      total += r;
      in->file_size = TO_LE32(total);

      // Déverrouiller le dernier bloc de données
      unlock_block(fd, last * 4096);
    }
    else
    {
      int32_t b = get_last_data_block_null(map, in);
      if (b == -2)
      {
        print_error("Erreur: pas de blocs de données libres disponibles");
        break;
      }

      // Verrouiller le nouveau bloc de données
      if (lock_block(fd, b * 4096, F_WRLCK) < 0)
      {
        print_error("Erreur lors du verrouillage du bloc de données");
        unlock_block(fd, inode_offset);
        close_fs(fd, map, size);
        return 1;
      }

      struct data_block *db = get_data_block(map, b);
      memset(db->data, 0, sizeof(db->data));
      memcpy(db->data, buf, r);
      calcul_sha1(db->data, 4000, db->sha1);
      db->type = TO_LE32(5);
      total += r;
      in->file_size = TO_LE32(total);

      // Déverrouiller le nouveau bloc de données
      unlock_block(fd, b * 4096);
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

int cmd_fsck(const char *fsname)
{
  sha1_queue_t queue = {.head = 0, .tail = 0, .done = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
  pthread_t threads[NB_THREADS];
  for (int i = 0; i < NB_THREADS; i++)
    pthread_create(&threads[i], NULL, sha1_worker, &queue);

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
        in->flags &= ~(1 << 3);
        in->flags &= ~(1 << 4);
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
        if ((int32_t)FROM_LE32(in->double_indirect_block) >= 0)
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
    
    unlock_block(fd, i * 4096);    

    pthread_mutex_lock(&queue.mutex);
    int next = (queue.tail + 1) % SHA1_QUEUE_SIZE;
    while (next == queue.head)
    { // queue pleine
      pthread_mutex_unlock(&queue.mutex);
      sched_yield();
      pthread_mutex_lock(&queue.mutex);
      next = (queue.tail + 1) % SHA1_QUEUE_SIZE;
    }
    queue.tasks[queue.tail] = (sha1_task_t){.data = bb, .len = 4000, .block_num = i, .block_type = bb->type};
    memcpy(queue.tasks[queue.tail].sha1_ref, bb->sha1, 20);
    queue.tail = next;
    pthread_cond_signal(&queue.cond);
    pthread_mutex_unlock(&queue.mutex);
  }

  // Signaler la fin
  pthread_mutex_lock(&queue.mutex);
  queue.done = 1;
  pthread_cond_broadcast(&queue.cond);
  pthread_mutex_unlock(&queue.mutex);

  // Attendre la fin des threads
  for (int i = 0; i < NB_THREADS; i++)
    pthread_join(threads[i], NULL);

  close_fs(fd, map, size);
  return 0;
}

// int cmd_mount(int argc, char *argv[])
// {
//   printf("Exécution de la commande mount\n");
//   return 0;
// }

int cmd_find(const char *fsname, const char *type, const char *name, int days, const char *date_type)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  time_t now = time(NULL);

  for (int i = 0; i < nbi; i++)
  {
    struct inode *in = get_inode(map, 1 + nb1 + i);
    if (!(FROM_LE32(in->flags) & 1)) // Ignorer les inodes non allouées
      continue;

    // Verrouiller l'inode en lecture
    int inode_offset = (1 + nb1) * 4096 + (int64_t)(in - (struct inode *)map);
    if (lock_block(fd, inode_offset, F_RDLCK) < 0)
    {
      print_error("Erreur lors du verrouillage de l'inode");
      close_fs(fd, map, size);
      return 1;
    }

    // Filtrer par type
    if (type)
    {
      bool is_dir = (FROM_LE32(in->flags) & (1 << 5)) != 0;
      if ((strcmp(type, "f") == 0 && is_dir) || (strcmp(type, "d") == 0 && !is_dir))
      {
        unlock_block(fd, inode_offset); // Déverrouiller l'inode
        continue;
      }
    }

    // Filtrer par nom
    if (name && strstr(in->filename, name) == NULL)
    {
      unlock_block(fd, inode_offset); // Déverrouiller l'inode
      continue;
    }

    // Filtrer par date
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

    // Afficher le fichier correspondant
    printf("%s\n", in->filename);

    // Déverrouiller l'inode
    unlock_block(fd, inode_offset);
  }

  close_fs(fd, map, size);
  return 0;
}

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

int cmd_mkdir(const char *fsname, char *path)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  int val = create_directory(map, path);
  if (val == -1)
  {
    close_fs(fd, map, size);
    return print_error("Erreur lors de la création du répertoire");
  }

  close_fs(fd, map, size);
  return 0;
}

int cmd_rmdir(const char *fsname, const char *path)
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
  split_path(path, parent_path, dir_name);

  int val = -1;
  struct inode *in = NULL;
  struct inode *in2 = NULL;
  int val2 = -1;

  boucle:
  if (strlen(parent_path) == 0)
  {
    val = find_inode_racine(map, nb1, nbi, dir_name, true);
    if (val == -1)
    {
      close_fs(fd, map, size);
      return print_error("Erreur: répertoire inexistant");
    }
    in = get_inode(map, val);
    delete_children(in, map);
    delete_inode(in, map, val);
  }
  else if(strlen(parent_path) > 0 && strlen(dir_name) > 0)
  {
    val = find_inode_folder(map, nb1, nbi, parent_path);
    if (val == -1)
    {
      close_fs(fd, map, size);
      return print_error("Erreur: répertoire parent inexistant");
    }
    in = get_inode(map, val);
    val2 = find_file_folder_from_inode(map, in, dir_name, true);
    if (val2 == -1)
    {
      close_fs(fd, map, size);
      return print_error("Erreur: répertoire à supprimer inexistant");
    }
    in2 = get_inode(map, val2);
    delete_children(in2, map);
    delete_inode(in2, map, val2);
    delete_separte_inode(in, val2);
  }else
  {
    printf("parent_path = %s\n", parent_path);
    printf("dir_name = %s\n", dir_name);
    split_path(parent_path, parent_path, dir_name);
    printf("parent_path = %s\n", parent_path);
    printf("dir_name = %s\n", dir_name);
    goto boucle;
  }

  close_fs(fd, map, size);
  return 0;
}

int cmd_mv(const char *fsname, const char *oldpath, const char *newpath)
{
  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);

  // Découper les chemins source et destination
  char old_parent_path[256], old_name[256];
  char new_parent_path[256], new_name[256];
  split_path(oldpath, old_parent_path, old_name); 
  split_path(newpath, new_parent_path, new_name);
  bool is_dir, is_dir2;

  is_dir = is_folder_path(old_parent_path, old_name);
  is_dir2 = is_folder_path(new_parent_path, new_name);
  
  int val1_1 = -1;
  int val1_2 = -1;
  int val2_1 = -1;
  int val2_2 = -1;

  // Trouver le répertoire parent source et destination
  struct inode *old_parent = NULL;
  struct inode *new_parent = NULL;
  struct inode *old_parent2 = NULL;

  if(is_dir)
  {
    split_path(old_parent_path, old_parent_path, old_name);
    if(strlen(old_parent_path) == 0)
    {
      val1_2 = find_inode_racine(map, nb1, nbi, old_name, true);
      if (val1_2 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire inexistant");
      }
      old_parent2 = get_inode(map, val1_2); //dossier racine
    }
    else{
      val1_1 = find_inode_folder(map, nb1, nbi, old_parent_path);
      if (val1_1 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire parent inexistant");
      }
      old_parent =  get_inode(map, val1_1); //avant dernier dossier
      val1_2 = find_file_folder_from_inode(map, old_parent, old_name, true);
      if (val1_2 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire à déplacer inexistant");
      }
      old_parent2 = get_inode(map, val1_2); //dernier dossier
    }
  }
  else
  {
    if(strlen(old_parent_path) == 0)
    {
      val1_2 = find_inode_racine(map, nb1, nbi, old_name, false);
      if (val1_2 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: fichier inexistant");
      }
      old_parent2 = get_inode(map, val1_1); //fichier racine
    }
    else
    {
      val1_1 = find_inode_folder(map, nb1, nbi, old_parent_path);
      if (val1_1 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire parent inexistant");
      }
      old_parent =  get_inode(map, val1_1); //avant dernier dossier
      val1_2 = find_file_folder_from_inode(map, old_parent, old_name, false);
      if (val1_2 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: fichier à déplacer inexistant");
      }
      old_parent2 = get_inode(map, val1_2); //dernier fichier
    }
  }

  if(is_dir2)
  {
    split_path(new_parent_path, new_parent_path, new_name);
    if(strlen(new_parent_path) == 0)
    {
      val2_2 = find_inode_racine(map, nb1, nbi, new_name, true);
      if (val2_2 != -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire existe déjà");
      }
    }
    else{
      val2_1 = find_inode_folder(map, nb1, nbi, new_parent_path);
      if (val2_1 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire parent inexistant");
      }
      new_parent =  get_inode(map, val2_1); //avant dernier dossier
      val2_2 = find_file_folder_from_inode(map, new_parent, new_name, true);
      if (val2_2 != -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire existe déjà");
      }
    }
  }
  else
  {
    if(strlen(new_parent_path) == 0)
    {
      val2_2 = find_inode_racine(map, nb1, nbi, new_name, false);
      if (val2_2 != -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: fichier existe déjà");
      }
    }
    else
    {
      val2_1 = find_inode_folder(map, nb1, nbi, new_parent_path);
      if (val2_1 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire parent inexistant");
      }
      new_parent =  get_inode(map, val2_1); //avant dernier dossier
      val2_2 = find_file_folder_from_inode(map, new_parent, new_name, false);
      if (val2_2 != -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: fichier existe déjà");
      }
    }
  }

  if(((!is_dir && !is_dir2) || (is_dir && is_dir2)) && (strcmp(old_parent_path, new_parent_path) == 0)) //rename
  {
    if(strlen(new_name) > 0)
    {
      strncpy(old_parent2->filename, new_name, 255);
      old_parent2->filename[255] = '\0';
    }
    old_parent2->modification_time = TO_LE32(time(NULL));
    calcul_sha1(old_parent2, 4000, old_parent2->sha1);
  }
  else if((!is_dir && !is_dir2) || (is_dir && is_dir2)) 
  {
    if(strlen(new_name) > 0)
    {
      strncpy(old_parent2->filename, new_name, 255);
      old_parent2->filename[255] = '\0';
    }
    old_parent2->modification_time = TO_LE32(time(NULL));
    calcul_sha1(old_parent2, 4000, old_parent2->sha1);
    if(strlen(new_parent_path) == 0)
    {
      old_parent2->modification_time = TO_LE32(time(NULL));
      calcul_sha1(old_parent2, 4000, old_parent2->sha1);
      old_parent2->profondeur = TO_LE32(0);
    }
    else
    {
      old_parent2->profondeur = TO_LE32(FROM_LE32(new_parent->profondeur) + 1);
      add_inode(new_parent, val1_2);
    } 
    old_parent2->modification_time = TO_LE32(time(NULL));
    calcul_sha1(old_parent2, 4000, old_parent2->sha1);
    if(strlen(old_parent_path) != 0)
    {
      delete_separte_inode(old_parent, val1_2);
    }
  }
  else if(!is_dir && is_dir2) 
  {
    if(strlen(new_parent_path) == 0)
    {
      old_parent2->profondeur = TO_LE32(0);
    }
    else
    {
      old_parent2->profondeur = TO_LE32(FROM_LE32(new_parent->profondeur) + 1);
      add_inode(new_parent, val1_2);
    } 
    old_parent2->modification_time = TO_LE32(time(NULL));
    calcul_sha1(old_parent2, 4000, old_parent2->sha1);
    if(strlen(old_parent_path) != 0)
    {
      delete_separte_inode(old_parent, val1_2);
    }
  }
  else
  {
    close_fs(fd, map, size);
    return print_error("Erreur: déplacement impossible dossier dans fichier");
  }

  close_fs(fd, map, size);

  return 0;
}