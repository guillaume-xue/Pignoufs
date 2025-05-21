#include "../include/structures.h"
#include "../include/utilitaires.h"

// Suppression récursive des enfants d'un répertoire
void delete_children(struct inode *dir, uint8_t *map, int fd, size_t size)
{
  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  for (int i = 0; i < 900; i++)
  {
    int32_t child_idx = FROM_LE32(dir->direct_blocks[i]);
    if (child_idx == -1)
      continue;
    struct inode *child = get_inode(map, child_idx);
    if ((FROM_LE32(child->flags) >> 5) & 1)
    {
      // Si c'est un sous-répertoire, suppression récursive
      delete_children(child, map, fd, size);
    }
    // Vérifie les permissions et les locks :
    // On ne supprime que si l'inode a le droit d'écriture et n'est pas locké en lecture ou écriture
    if (!((FROM_LE32(child->flags) >> 2) & 1) || ((FROM_LE32(child->flags) >> 3) & 1) || ((FROM_LE32(child->flags) >> 4) & 1))
      continue;
    delete_inode(child, map, child_idx, fd, size);
    dir->direct_blocks[i] = TO_LE32(-1);
  }
  calcul_sha1(dir, 4000, dir->sha1);
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

  // Séparer le chemin parent et le nom du répertoire à supprimer
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

    // Vérifie les permissions et les locks sur le dossier à supprimer
    if (!((FROM_LE32(in->flags) >> 2) & 1) || ((FROM_LE32(in->flags) >> 3) & 1) || ((FROM_LE32(in->flags) >> 4) & 1))
    {
      close_fs(fd, map, size);
      return print_error("Erreur: pas de droit d'écriture ou dossier verrouillé (lecture/écriture)");
    }

    delete_children(in, map, fd, size);
    delete_inode(in, map, val, fd, size);
  }
  else if (strlen(parent_path) > 0 && strlen(dir_name) > 0)
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

    // Vérifie les permissions et les locks sur le dossier à supprimer
    if (!((FROM_LE32(in2->flags) >> 2) & 1) || ((FROM_LE32(in2->flags) >> 3) & 1) || ((FROM_LE32(in2->flags) >> 4) & 1))
    {
      close_fs(fd, map, size);
      return print_error("Erreur: pas de droit d'écriture ou dossier verrouillé (lecture/écriture)");
    }

    delete_children(in2, map, fd, size);
    delete_inode(in2, map, val2, fd, size);
    delete_separte_inode(in, val2);
  }
  else
  {
    // Cas de chemin complexe, on continue à découper le chemin
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