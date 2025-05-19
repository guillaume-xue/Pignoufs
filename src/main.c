#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../include/commands.h"
#include "../include/sha1.h"

static void print_usage()
{
  fprintf(stderr, "Usage: <nom_de_commande> <fsname> [options]\n");
  fprintf(stderr, "Commandes disponibles : mkfs, ls, df, cp, rm, lock, chmod, cat, input, add, addinput, fsck, mount.\n");
  fprintf(stderr, "Options disponibles : -v (verbose), -h (help), etc.\n");
}

int main(int argc, char *argv[])
{

  if (argc < 2)
  {
    print_usage();
    return 1;
  }

  // Récupérer le nom de l’exécutable (argv[0]) et ignorer le chemin
  char *command = strrchr(argv[0], '/');
  command = (command) ? command + 1 : argv[0]; // Si '/' trouvé, avancer d’un caractère

  // Vérifier les options globales
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-h") == 0)
    {
      print_usage();
      return 0;
    }
    else if (strcmp(argv[i], "-v") == 0)
    {
      printf("-v\n");
    }
  }

  const char *fsname = argv[1];
  if (strncmp(fsname, "//", 2) == 0)
  {
    fprintf(stderr, "Erreur: Le nom de fichier externe ne doit pas commencer par \"//\".\n");
    return 1;
  }

  // Appeler la commande correspondante
  if (strcmp(command, "mkfs") == 0)
  {
    if (argc < 4)
    {
      fprintf(stderr, "Usage: mkfs <fsname> <nbi> <nba>\n");
      return 1;
    }
    char *endptr;
    int nbi = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0')
    {
      fprintf(stderr, "Erreur: %s n'est pas un nombre valide.\n", argv[2]);
      return 1;
    }
    int nba = strtol(argv[3], &endptr, 10);
    if (*endptr != '\0')
    {
      fprintf(stderr, "Erreur: %s n'est pas un nombre valide.\n", argv[3]);
      return 1;
    }
    return cmd_mkfs(fsname, nbi, nba);
  }
  else if (strcmp(command, "ls") == 0)
  {
    const char *filename = NULL;
    if (argc > 2)
    {
      filename = argv[2];
      if (strncmp(filename, "//", 2) != 0)
      {
        fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
        return 1;
      }
      filename += 2; // Ignorer les deux premiers caractères
    }
    return cmd_ls(fsname, filename);
  }
  else if (strcmp(command, "tree") == 0)
  {
    const char *filename = NULL;
    if (argc > 2)
    {
      filename = argv[2];
      if (strncmp(filename, "//", 2) != 0)
      {
        fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
        return 1;
      }
      filename += 2; // Ignorer les deux premiers caractères
    }
    return cmd_tree(fsname, filename);
  }
  else if (strcmp(command, "df") == 0)
  {
    return cmd_df(fsname);
  }
  else if (strcmp(command, "cp") == 0)
  {
    bool mode1 = false, mode2 = false, directory = false;
    if (argc < 4)
    {
      fprintf(stderr, "Erreur : la commande 'cp' nécessite au moins 3 arguments.\n");
      return 1;
    }
    const char *filename1 = argv[2];
    if (strncmp(filename1, "//", 2) == 0)
    {
      filename1 += 2;
      mode1 = true;
    }
    const char *filename2 = argv[3];
    if (strncmp(filename2, "//", 2) == 0)
    {
      filename2 += 2;
      mode2 = true;
    }
    if (!mode1 && !mode2)
    {
      fprintf(stderr, "Erreur: Les deux fichiers ne peuvent pas être externes.\n");
      return 1;
    }
    if (argc > 4)
    {
      if (strcmp(argv[4], "-r") == 0)
      {
        directory = true;
      }
      else
      {
        fprintf(stderr, "Erreur: L'option est inconnue.\n");
        return 1;
      }
    }
    return cmd_cp(fsname, filename1, filename2, mode1, mode2, directory);
  }
  else if (strcmp(command, "rm") == 0)
  {
    if (argc < 3)
    {
      fprintf(stderr, "Erreur : la commande 'rm' nécessite au moins 2 arguments.\n");
      return 1;
    }
    const char *filename = argv[2];
    if (strncmp(filename, "//", 2) != 0)
    {
      fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
      return 1;
    }
    filename += 2; // Ignorer les deux premiers caractères
    return cmd_rm(fsname, filename);
  }
  else if (strcmp(command, "lock") == 0)
  {
    if (argc < 4)
    {
      fprintf(stderr, "Erreur : la commande 'lock' nécessite au moins 3 arguments.\n");
      return 1;
    }
    const char *filename = argv[2];
    if (strncmp(filename, "//", 2) != 0)
    {
      fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
      return 1;
    }
    filename += 2;
    const char *arg = argv[3];
    if (strcmp(arg, "r") == 0 || strcmp(arg, "w") == 0)
    {
      return cmd_lock(fsname, filename, arg);
    }
    else
    {
      fprintf(stderr, "Erreur: L'argument de lock doit être 'r' ou 'w'.\n");
      return 1;
    }
  }
  else if (strcmp(command, "chmod") == 0)
  {
    if (argc < 3)
    {
      fprintf(stderr, "Erreur : la commande 'chmod' nécessite au moins 2 arguments.\n");
      return 1;
    }
    const char *filename = argv[2];
    if (strncmp(filename, "//", 2) != 0)
    {
      fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
      return 1;
    }
    filename += 2;
    const char *arg = argv[3];
    if (strcmp(arg, "+r") == 0 || strcmp(arg, "-r") == 0 || strcmp(arg, "+w") == 0 || strcmp(arg, "-w") == 0)
    {
      return cmd_chmod(fsname, filename, arg);
    }
    else
    {
      fprintf(stderr, "Erreur: L'argument de chmod doit être +r, -r, +w ou -w.\n");
      return 1;
    }
  }
  else if (strcmp(command, "cat") == 0)
  {
    if (argc < 3)
    {
      fprintf(stderr, "Erreur : la commande 'cat' nécessite au moins 2 arguments.\n");
      return 1;
    }
    const char *filename = argv[2];
    if (strncmp(filename, "//", 2) != 0)
    {
      fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
      return 1;
    }
    filename += 2;
    return cmd_cat(fsname, filename);
  }
  else if (strcmp(command, "input") == 0)
  {
    if (argc < 3)
    {
      fprintf(stderr, "Erreur : la commande 'input' nécessite au moins 2 arguments.\n");
      return 1;
    }
    const char *filename = argv[2];
    if (strncmp(filename, "//", 2) != 0)
    {
      fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
      return 1;
    }
    filename += 2;
    return cmd_input(fsname, filename);
  }
  else if (strcmp(command, "add") == 0)
  {
    if (argc < 4)
    {
      fprintf(stderr, "Erreur : la commande 'add' nécessite au moins 3 arguments.\n");
      return 1;
    }
    if (strncmp(argv[2], "//", 2) == 0)
    {
      fprintf(stderr, "Erreur: Le nom de fichier externe ne doit pas commencer par \"//\".\n");
      return 1;
    }
    const char *filename_ext = argv[2];
    if (strncmp(argv[3], "//", 2) != 0)
    {
      fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
      return 1;
    }
    argv[3] += 2;
    const char *filename_int = argv[3];
    return cmd_add(fsname, filename_ext, filename_int);
  }
  else if (strcmp(command, "addinput") == 0)
  {
    if (argc < 3)
    {
      fprintf(stderr, "Erreur : la commande 'addinput' nécessite au moins 2 arguments.\n");
      return 1;
    }
    const char *filename = argv[2];
    if (strncmp(filename, "//", 2) != 0)
    {
      fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
      return 1;
    }
    return cmd_addinput(fsname, filename);
  }
  else if (strcmp(command, "fsck") == 0)
  {
    return cmd_fsck(fsname);
  }
  else if (strcmp(command, "mount") == 0)
  {
    return cmd_mount(argc, argv);
  }
  else if (strcmp(command, "find") == 0)
  {
    const char *type = NULL, *name = NULL, *date_type = NULL;
    int days = 0;

    for (int i = 2; i < argc; i++)
    {
      if (strcmp(argv[i], "-type") == 0 && i + 1 < argc)
        type = argv[++i];
      else if (strcmp(argv[i], "-name") == 0 && i + 1 < argc)
        name = argv[++i];
      else if (strcmp(argv[i], "-ctime") == 0 && i + 1 < argc)
        date_type = "ctime", days = atoi(argv[++i]);
      else if (strcmp(argv[i], "-mtime") == 0 && i + 1 < argc)
        date_type = "mtime", days = atoi(argv[++i]);
      else if (strcmp(argv[i], "-atime") == 0 && i + 1 < argc)
        date_type = "atime", days = atoi(argv[++i]);
    }
    return cmd_find(fsname, type, name, days, date_type);
  }
  else if (strcmp(command, "grep") == 0)
  {
    if (argc < 3)
    {
      fprintf(stderr, "Erreur : la commande 'grep' nécessite un motif.\n");
      return 1;
    }

    const char *pattern = argv[2];
    return cmd_grep(fsname, pattern);
  }
  else if (strcmp(command, "mkdir") == 0)
  {
    if (argc < 3)
    {
      fprintf(stderr, "Erreur : la commande 'mkdir' nécessite au moins 2 arguments.\n");
      return 1;
    }
    const char *path = argv[2];
    if (strncmp(path, "//", 2) != 0)
    {
      fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
      return 1;
    }
    path += 2; // Ignorer les deux premiers caractères
    if (strlen(path) == 0)
    {
      fprintf(stderr, "Erreur: Le nom de répertoire ne peut pas être vide.\n");
      return 1;
    }
    return cmd_mkdir(fsname, path);
  }
  else if (strcmp(command, "rmdir") == 0)
  {
    if (argc < 3)
    {
      fprintf(stderr, "Erreur : la commande 'rmdir' nécessite au moins 2 arguments.\n");
      return 1;
    }
    const char *path = argv[2];
    if (strncmp(path, "//", 2) != 0)
    {
      fprintf(stderr, "Erreur: Le nom de fichier interne doit commencer par \"//\".\n");
      return 1;
    }
    path += 2; // Ignorer les deux premiers caractères
    return cmd_rmdir(fsname, path);
  }
  else if (strcmp(command, "mv") == 0)
  {
    if (argc < 4)
    {
      fprintf(stderr, "Erreur : la commande 'mv' nécessite au moins 3 arguments.\n");
      return 1;
    }
    const char *oldpath = argv[2];
    const char *newpath = argv[3];
    if (strncmp(oldpath, "//", 2) != 0 || strncmp(newpath, "//", 2) != 0)
    {
      fprintf(stderr, "Erreur: Les noms de fichiers internes doivent commencer par \"//\".\n");
      return 1;
    }
    oldpath += 2; // Ignorer les deux premiers caractères
    newpath += 2; // Ignorer les deux premiers caractères
    return cmd_mv(fsname, oldpath, newpath);
  }
  else
  {
    fprintf(stderr, "Erreur : commande inconnue '%s'\n", command);
    print_usage();
    return 1;
  }
  return 0;
}