#include "../include/structures.h"
#include "../include/utilitaires.h"

static void print_ls(struct inode *in)
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
  printf("%s %10u %s %s%s\n", perm, in->file_size, buf, in->filename, (((FROM_LE32(in->flags) >> 5) & 1) ? "/" : ""));
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

  if (filename == NULL)
  { // racine print
    for (int i = 0; i < nbi; i++)
    {
      struct inode *in = get_inode(map, 1 + nb1 + i);
      if ((in->flags & 1) && FROM_LE32(in->profondeur) == 0)
      {
        if (argument != NULL)
        {
          if (strcmp(argument, "-l") == 0)
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
  else
  {
    char parent_path[256], dir_name[256];
    split_path(filename, parent_path, dir_name);
    bool is_dir = is_folder_path(parent_path, dir_name);
    int val = -1;
    struct inode *in = NULL;

    if (is_dir)
    {
      val = find_inode_folder(map, nb1, nbi, parent_path);
      if (val == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: le répertoire parent n'existe pas");
      }
      in = get_inode(map, val);
    }
    else
    {
      if (strlen(parent_path) == 0)
      {
        // Recherche l'inode à la racine
        val = find_inode_racine(map, nb1, nbi, dir_name, false);
        if (val == -1)
        {
          close_fs(fd, map, size);
          return print_error("Erreur: le fichier n'existe pas");
        }
        in = get_inode(map, val);
      }
      else
      {
        // Recherche l'inode dans un sous-répertoire
        val = find_inode_folder(map, nb1, nbi, parent_path);
        if (val == -1)
        {
          close_fs(fd, map, size);
          return print_error("Erreur: le répertoire parent du fichier n'existe pas");
        }
        in = get_inode(map, val);
        val = find_file_folder_from_inode(map, in, dir_name, false);
        if (val == -1)
        {
          close_fs(fd, map, size);
          return print_error("Erreur: le fichier n'existe pas");
        }
        in = get_inode(map, val);
      }
    }

    // Vérifie les permissions et les locks :
    // On n'affiche que si l'inode a le droit de lecture et n'est pas locké en lecture ou écriture
    if (!((FROM_LE32(in->flags) >> 1) & 1) || ((FROM_LE32(in->flags) >> 3) & 1) || ((FROM_LE32(in->flags) >> 4) & 1))
    {
      close_fs(fd, map, size);
      return print_error("Erreur: pas de droit de lecture ou fichier/dossier verrouillé (lecture/écriture)");
    }

    // Si c'est un dossier, affiche son contenu
    if (FROM_LE32(in->flags >> 5) & 1)
    {
      for (int i = 0; i < 900; i++)
      {
        if (in->direct_blocks[i] != -1)
        {
          struct inode *child_inode = get_inode(map, FROM_LE32(in->direct_blocks[i]));
          if (child_inode->flags & 1)
          {
            if (argument != NULL)
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
      // Sinon, affiche le fichier lui-même
      if (argument != NULL)
      {
        if (strcmp(argument, "-l") == 0)
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

  close_fs(fd, map, size);
  return 0;
}
