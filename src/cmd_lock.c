#include "../include/structures.h"
#include "../include/utilitaires.h"

// Variable globale pour contrôler la boucle de verrouillage
volatile sig_atomic_t keep_running = 1;

// Handler pour SIGTERM : arrête la boucle
void handle_sigterm(int sig)
{
  (void)sig;
  keep_running = 0;
}

// Handler pour SIGINT : relaye en SIGTERM
void handle_sigint(int sig)
{
  (void)sig;
  raise(SIGTERM);
}

// Fonction pour appliquer un verrouillage (lecture ou écriture) sur un fichier ou dossier
int cmd_lock(const char *fsname, const char *filename, const char *arg)
{
  // Installation des gestionnaires de signaux pour gérer l'arrêt propre
  signal(SIGTERM, handle_sigterm);
  signal(SIGINT, handle_sigint);

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

  // Recherche l'inode correspondant au fichier/dossier cible
  if (strlen(parent_path) == 0)
  {
    val = find_inode_racine(map, nb1, nbi, dir_name, false);
    if (val < 0)
    {
      print_error("Fichier introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
  }
  else
  {
    val = find_inode_folder(map, nb1, nbi, parent_path);
    if (val < 0)
    {
      print_error("Répertoire introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
    val = find_file_folder_from_inode(map, in, dir_name, false);
    if (val < 0)
    {
      print_error("Fichier introuvable");
      close_fs(fd, map, size);
      return 1;
    }
    in = get_inode(map, val);
  }

  // Applique le verrouillage demandé
  if (strcmp(arg, "r") == 0)
  {
    // Lock lecture (bit 3)
    in->flags |= (1 << 3);
    while (keep_running)
    {
      sleep(1);
    }
    // Déverrouille à la fin
    in->flags &= ~(1 << 3);
    printf("SIGTERM reçu. Fin du programme.\n");
  }
  else if (strcmp(arg, "w") == 0)
  {
    // Lock écriture (bit 4)
    in->flags |= (1 << 4);
    while (keep_running)
    {
      sleep(1);
    }
    // Déverrouille à la fin
    in->flags &= ~(1 << 4);
    printf("SIGTERM reçu. Fin du programme.\n");
  }
  return 0;
}