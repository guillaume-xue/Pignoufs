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
    int val = find_inode_folder(map, nb1, nbi, parent_path);
    struct inode *in = get_inode(map, val);
    if (in == NULL)
    {
      close_fs(fd, map, size);
      return print_error("Erreur: le répertoire n'existe pas");
    }
    struct inode *file = NULL;
    if (strlen(dir_name) == 0)
    {
      file = in;
    }
    else
    {
      val = find_file_folder_from_inode(map, in, dir_name, false);
      if (val == -1)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: le fichier n'existe pas");
      }
      file = get_inode(map, val);
      if (file == NULL)
      {
        close_fs(fd, map, size);
        return print_error("Erreur: le fichier n'existe pas");
      }
    }
    if ((file->flags >> 5) & 1)
    {
      for (int i = 0; i < 900; i++)
      {
        if (file->direct_blocks[i] != -1)
        {
          struct inode *child_inode = get_inode(map, FROM_LE32(file->direct_blocks[i]));
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
