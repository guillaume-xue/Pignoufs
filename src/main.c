#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/commands.h"

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
  else if (strcmp(command, "df") == 0)
  {
    return cmd_df(fsname);
  }
  else if (strcmp(command, "cp") == 0)
  {
    return cmd_cp(argc, argv);
  }
  else if (strcmp(command, "rm") == 0)
  {
    return cmd_rm(argc, argv);
  }
  else if (strcmp(command, "lock") == 0)
  {
    return cmd_lock(argc, argv);
  }
  else if (strcmp(command, "chmod") == 0)
  {
    return cmd_chmod(argc, argv);
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
    return cmd_input(argc, argv);
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
    return cmd_addinput(argc, argv);
  }
  else if (strcmp(command, "fsck") == 0)
  {
    return cmd_fsck(argc, argv);
  }
  else if (strcmp(command, "mount") == 0)
  {
    return cmd_mount(argc, argv);
  }
  else
  {
    fprintf(stderr, "Erreur : commande inconnue '%s'\n", command);
    print_usage();
    return 1;
  }
  return 0;
}