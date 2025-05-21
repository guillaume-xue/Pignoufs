#include "../include/structures.h"
#include "../include/utilitaires.h"

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
    printf("    ");

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