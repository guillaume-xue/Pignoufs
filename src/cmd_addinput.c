#include "../include/structures.h"
#include "../include/utilitaires.h"

// Fonction pour ajouter le contenu de l'entrée standard dans un fichier du système de fichiers interne
int cmd_addinput(const char *fsname, const char *filename)
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
    // Si le fichier doit être ajouté à la racine
    val = find_inode_racine(map, nb1, nbi, dir_name, false);
    if (val == -1)
    {
      // Crée le fichier s'il n'existe pas
      val = create_file(map, dir_name);
    }
    in = get_inode(map, val);
  }
  else
  {
    // Si le fichier doit être ajouté dans un sous-répertoire
    val = find_inode_folder(map, nb1, nbi, parent_path);
    if (val == -1)
    {
      // Si le répertoire parent n'existe pas, le créer
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
      // Si le répertoire parent existe
      in = get_inode(map, val);
      val = find_file_folder_from_inode(map, in, dir_name, false);
      if (val == -1)
      {
        // Crée le fichier s'il n'existe pas dans le répertoire
        val = create_file(map, dir_name);
        struct inode *in2 = get_inode(map, val);
        in2->profondeur = TO_LE32(FROM_LE32(in->profondeur) + 1);
        add_inode(in, val);
        in = in2;
      }
      else
      {
        // Le fichier existe déjà
        in = get_inode(map, val);
      }
    }
  }

  // Verrouille l'inode pour éviter les accès concurrents
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

  // Boucle de lecture depuis l'entrée standard et écriture dans le fichier interne
  while ((r = read(STDIN_FILENO, buf, 4000)) > 0)
  {
    uint32_t offset = (total % 4000);
    if (offset > 0)
    {
      // Si le dernier bloc de données n'est pas plein, le compléter
      int32_t last = get_last_data_block(map, in);

      // Verrouille le dernier bloc de données
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

      // Déverrouille le dernier bloc de données
      unlock_block(fd, last * 4096);
    }
    else
    {
      // Sinon, alloue un nouveau bloc de données
      int32_t b = get_last_data_block_null(map, in);
      if (b == -2)
      {
        print_error("Erreur: pas de blocs de données libres disponibles");
        break;
      }

      // Verrouille le nouveau bloc de données
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

      // Déverrouille le nouveau bloc de données
      unlock_block(fd, b * 4096);
    }
  }

  // Met à jour la taille et la date de modification du fichier
  in->file_size = TO_LE32(total);
  in->modification_time = TO_LE32(time(NULL));
  calcul_sha1(in, 4000, in->sha1);

  // Déverrouille l'inode
  unlock_block(fd, inode_offset);

  // Ferme le système de fichiers
  close_fs(fd, map, size);
  return 0;
}