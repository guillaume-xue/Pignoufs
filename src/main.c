#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/commands.h"

void print_usage()
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

  // Appeler la commande correspondante
  if (strcmp(command, "mkfs") == 0)
  {
    cmd_mkfs(argc, argv);
  }
  else if (strcmp(command, "ls") == 0)
  {
    cmd_ls(argc, argv);
  }
  else if (strcmp(command, "df") == 0)
  {
    cmd_df(argc, argv);
  }
  else if (strcmp(command, "cp") == 0)
  {
    cmd_cp(argc, argv);
  }
  else if (strcmp(command, "rm") == 0)
  {
    cmd_rm(argc, argv);
  }
  else if (strcmp(command, "lock") == 0)
  {
    cmd_lock(argc, argv);
  }
  else if (strcmp(command, "chmod") == 0)
  {
    cmd_chmod(argc, argv);
  }
  else if (strcmp(command, "cat") == 0)
  {
    cmd_cat(argc, argv);
  }
  else if (strcmp(command, "input") == 0)
  {
    cmd_input(argc, argv);
  }
  else if (strcmp(command, "add") == 0)
  {
    cmd_add(argc, argv);
  }
  else if (strcmp(command, "addinput") == 0)
  {
    cmd_addinput(argc, argv);
  }
  else if (strcmp(command, "fsck") == 0)
  {
    cmd_fsck(argc, argv);
  }
  else if (strcmp(command, "mount") == 0)
  {
    cmd_mount(argc, argv);
  }
  else
  {
    fprintf(stderr, "Erreur : commande inconnue '%s'\n", command);
    print_usage();
    return 1;
  }

  return 0;
}