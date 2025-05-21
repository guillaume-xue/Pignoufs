#include "../include/structures.h"
#include "../include/utilitaires.h"

// Fonction pour afficher le contenu d'un fichier du système de fichiers interne sur la sortie standard
int cmd_cat(const char *fsname, const char *filename)
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

  char parent_path[256], dir_name[256];
  // Sépare le chemin interne en chemin parent et nom du fichier
  split_path(filename, parent_path, dir_name);

  struct inode *in = NULL;
  int val = -1;

  if (strlen(parent_path) == 0)
  {
    // Recherche le fichier à la racine
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
    // Recherche le répertoire parent puis le fichier
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

  // Vérifie les droits de lecture et que le fichier n'est pas verrouillé
  if(!((in->flags >> 1) & 1) || ((in->flags >> 3) & 1))
  {
    print_error("Erreur: pas de droit de lecture/fichier locké");
    close_fs(fd, map, size);
    return 1;
  }
  uint32_t file_size = FROM_LE32(in->file_size);
  uint32_t remaining_data = file_size;

  // 1) Parcours des blocs directs
  for (int i = 0; i < 900 && remaining_data > 0; i++)
  {
    int32_t b = FROM_LE32(in->direct_blocks[i]);
    if (b < 0)
      break;

    // Verrouille le bloc source pour lecture
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

    // Déverrouille le bloc source
    unlock_block(fd, b * 4096);
  }

  // Récupère le bloc double indirect si besoin
  int32_t dib = FROM_LE32(in->double_indirect_block);
  if (remaining_data == 0 || dib < 0)
  {
    goto end;
  }

  // 2) Parcours des blocs indirects ou double indirects
  struct address_block *dbl = get_address_block(map, dib);
  if (FROM_LE32(dbl->type) == 6)
  {
    // Simple indirect
    for (int i = 0; i < 1000 && remaining_data > 0; i++)
    {
      int32_t db = FROM_LE32(dbl->addresses[i]);
      if (db < 0)
        continue; // Cas fichier à trou

      // Verrouille le bloc source
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
    // Double indirect
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

        // Verrouille le bloc source
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
  // Ferme le système de fichiers
  close_fs(fd, map, size);
  return 0;
}