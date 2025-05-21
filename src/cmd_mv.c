#include "../include/structures.h"
#include "../include/utilitaires.h"

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

  if (is_dir)
  {
    split_path(old_parent_path, old_parent_path, old_name);
    if (strlen(old_parent_path) == 0)
    {
      val1_2 = find_inode_racine(map, nb1, nbi, old_name, true);
      if (val1_2 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire inexistant");
      }
      old_parent2 = get_inode(map, val1_2); // dossier racine
    }
    else
    {
      val1_1 = find_inode_folder(map, nb1, nbi, old_parent_path);
      if (val1_1 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire parent inexistant");
      }
      old_parent = get_inode(map, val1_1); // avant dernier dossier
      val1_2 = find_file_folder_from_inode(map, old_parent, old_name, true);
      if (val1_2 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire à déplacer inexistant");
      }
      old_parent2 = get_inode(map, val1_2); // dernier dossier
    }
  }
  else
  {
    if (strlen(old_parent_path) == 0)
    {
      val1_2 = find_inode_racine(map, nb1, nbi, old_name, false);
      if (val1_2 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: fichier inexistant");
      }
      old_parent2 = get_inode(map, val1_1); // fichier racine
    }
    else
    {
      val1_1 = find_inode_folder(map, nb1, nbi, old_parent_path);
      if (val1_1 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire parent inexistant");
      }
      old_parent = get_inode(map, val1_1); // avant dernier dossier
      val1_2 = find_file_folder_from_inode(map, old_parent, old_name, false);
      if (val1_2 == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: fichier à déplacer inexistant");
      }
      old_parent2 = get_inode(map, val1_2); // dernier fichier
    }
  }

  if (is_dir2)
  {
    split_path(new_parent_path, new_parent_path, new_name);
    if (strlen(new_parent_path) == 0)
    {
      val2_2 = find_inode_racine(map, nb1, nbi, new_name, true);
      if (val2_2 != -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: répertoire existe déjà");
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
      new_parent = get_inode(map, val2_1); // avant dernier dossier
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
    if (strlen(new_parent_path) == 0)
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
      new_parent = get_inode(map, val2_1); // avant dernier dossier
      val2_2 = find_file_folder_from_inode(map, new_parent, new_name, false);
      if (val2_2 != -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: fichier existe déjà");
      }
    }
  }

  if (((!is_dir && !is_dir2) || (is_dir && is_dir2)) && (strcmp(old_parent_path, new_parent_path) == 0)) // rename
  {
    if (strlen(new_name) > 0)
    {
      strncpy(old_parent2->filename, new_name, 255);
      old_parent2->filename[255] = '\0';
    }
    old_parent2->modification_time = TO_LE32(time(NULL));
    calcul_sha1(old_parent2, 4000, old_parent2->sha1);
  }
  else if ((!is_dir && !is_dir2) || (is_dir && is_dir2))
  {
    if (strlen(new_name) > 0)
    {
      strncpy(old_parent2->filename, new_name, 255);
      old_parent2->filename[255] = '\0';
    }
    old_parent2->modification_time = TO_LE32(time(NULL));
    calcul_sha1(old_parent2, 4000, old_parent2->sha1);
    if (strlen(new_parent_path) == 0)
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
    if (strlen(old_parent_path) != 0)
    {
      delete_separte_inode(old_parent, val1_2);
    }
  }
  else if (!is_dir && is_dir2)
  {
    if (strlen(new_parent_path) == 0)
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
    if (strlen(old_parent_path) != 0)
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