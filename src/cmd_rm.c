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

  // Séparer le chemin parent et le nom du fichier à supprimer
  char parent_path[256], dir_name[256];
  split_path(filename, parent_path, dir_name);
  bool is_dir = is_folder_path(parent_path, dir_name);
  if (is_dir)
  {
    return print_error("Erreur: la cible ne doit pas être un dossier");
  }

  int val = -1;
  struct inode *in = NULL;
  struct inode *in2 = NULL;
  int val2 = -1;

  if (strlen(parent_path) == 0)
  {
    // Cas où le fichier est à la racine
    val2 = find_inode_racine(map, nb1, nbi, dir_name, false);
    if (val2 == -1)
    {
      close_fs(fd, map, size);
      return print_error("Erreur: répertoire inexistant");
    }
    in2 = get_inode(map, val2);
  }
  else
  {
    // Cas où le fichier est dans un sous-répertoire
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

  // Vérifie les permissions et les locks :
  // On ne supprime que si l'inode a le droit d'écriture et n'est pas locké en lecture ou écriture
  if (!((FROM_LE32(in2->flags) >> 2) & 1) || ((FROM_LE32(in2->flags) >> 3) & 1) || ((FROM_LE32(in2->flags) >> 4) & 1))
  {
    close_fs(fd, map, size);
    return print_error("Erreur: pas de droit d'écriture ou fichier verrouillé (lecture/écriture)");
  }

  // Supprime l'inode et libère les blocs associés
  delete_inode(in2, map, val2, fd, size);

  close_fs(fd, map, size);
  return 0;
}