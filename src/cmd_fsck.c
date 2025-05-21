#include "../include/structures.h"
#include "../include/utilitaires.h"

#define NB_THREADS 6

// Fonction principale de vérification et correction du système de fichiers (fsck)
int cmd_fsck(const char *fsname)
{
  // Initialisation de la file de tâches SHA1 et des threads de vérification
  sha1_queue_t queue = {.head = 0, .tail = 0, .done = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
  pthread_t threads[NB_THREADS];
  for (int i = 0; i < NB_THREADS; i++)
    pthread_create(&threads[i], NULL, sha1_worker, &queue);

  uint8_t *map;
  size_t size;
  // Ouvre le système de fichiers et mappe son contenu en mémoire
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  // Récupère les informations sur la structure du conteneur
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct pignoufs *sb = (struct pignoufs *)map;

  // Vérifie la magie du système de fichiers
  if (strncmp(sb->magic, "pignoufs", 8) != 0)
  {
    print_error("Erreur: le conteneur n'est pas un système de fichiers Pignoufs");
    close_fs(fd, map, size);
    return 1;
  }

  // Parcours de tous les blocs du système de fichiers
  for (int32_t i = 1; i < nbb; i++)
  {
    struct bitmap_block *bb = (struct bitmap_block *)((uint8_t *)map + (int64_t)(i) * 4096);
    if (i <= nb1)
    {
      // Vérifie le type des blocs de bitmap
      if (bb->type != TO_LE32(2))
      {
        print_error("Erreur: bloc de bitmap a un type incorrect");
        close_fs(fd, map, size);
        return 1;
      }
    }
    else if (i <= nb1 + nbi)
    {
      // Vérifie le type des blocs d'inode
      if (bb->type != TO_LE32(3))
      {
        print_error("Erreur: bloc d'inode a un type incorrect");
        close_fs(fd, map, size);
        return 1;
      }
      struct inode *in = (struct inode *)bb;
      if (in->flags & 1) // Si l'inode est allouée
      {
        // Vérifie que le bloc de l'inode est bien alloué dans le bitmap
        if (!check_bitmap_bit(map, i))
        {
          print_error("Erreur: inode a un bloc de données non alloué");
          close_fs(fd, map, size);
          return 1;
        }
        // Réinitialise les locks sur l'inode
        in->flags &= ~(1 << 3);
        in->flags &= ~(1 << 4);
        // Vérifie tous les blocs directs de l'inode
        for (int j = 0; j < 900; j++)
        {
          int32_t b = FROM_LE32(in->direct_blocks[j]);
          if (b < 0)
            break;
          if (!check_bitmap_bit(map, b))
          {
            print_error("Erreur: inode a un bloc de données non alloué");
            close_fs(fd, map, size);
            return 1;
          }
        }
        // Vérifie les blocs indirects/double indirects si présents
        if ((int32_t)FROM_LE32(in->double_indirect_block) >= 0)
        {
          struct address_block *dbl = get_address_block(map, FROM_LE32(in->double_indirect_block));
          if (FROM_LE32(dbl->type) == 6)
          {
            // Simple indirect
            for (int j = 0; j < 1000; j++)
            {
              int32_t db = FROM_LE32(dbl->addresses[j]);
              if (db < 0)
                break;
              if (!check_bitmap_bit(map, db))
              {
                print_error("Erreur: inode a un bloc de données indirect non alloué");
                close_fs(fd, map, size);
                return 1;
              }
            }
          }
          else
          {
            // Double indirect
            for (int j = 0; j < 1000; j++)
            {
              int32_t db = FROM_LE32(dbl->addresses[j]);
              if (db < 0)
                break;
              struct address_block *sib = get_address_block(map, db);
              for (int k = 0; k < 1000; k++)
              {
                int32_t db2 = FROM_LE32(sib->addresses[k]);
                if (db2 < 0)
                  break;
                if (!check_bitmap_bit(map, db2))
                {
                  print_error("Erreur: inode a un bloc de données indirect non alloué");
                  close_fs(fd, map, size);
                  return 1;
                }
              }
            }
          }
        }
      }
    }
    else
    {
      // Vérifie le type des blocs de données
      if (bb->type < TO_LE32(4) || bb->type > TO_LE32(7))
      {
        print_error("Erreur: bloc de données a un type incorrect");
        close_fs(fd, map, size);
        return 1;
      }
    }

    // Déverrouille le bloc après vérification
    unlock_block(fd, i * 4096);

    // Ajoute une tâche SHA1 à la file pour vérification asynchrone
    pthread_mutex_lock(&queue.mutex);
    int next = (queue.tail + 1) % SHA1_QUEUE_SIZE;
    while (next == queue.head)
    { // queue pleine
      pthread_mutex_unlock(&queue.mutex);
      sched_yield();
      pthread_mutex_lock(&queue.mutex);
      next = (queue.tail + 1) % SHA1_QUEUE_SIZE;
    }
    queue.tasks[queue.tail] = (sha1_task_t){.data = bb, .len = 4000, .block_num = i, .block_type = bb->type};
    memcpy(queue.tasks[queue.tail].sha1_ref, bb->sha1, 20);
    queue.tail = next;
    pthread_cond_signal(&queue.cond);
    pthread_mutex_unlock(&queue.mutex);
  }

  // Signale la fin des tâches SHA1 aux threads
  pthread_mutex_lock(&queue.mutex);
  queue.done = 1;
  pthread_cond_broadcast(&queue.cond);
  pthread_mutex_unlock(&queue.mutex);

  // Attend la fin de tous les threads SHA1
  for (int i = 0; i < NB_THREADS; i++)
    pthread_join(threads[i], NULL);

  // Ferme le système de fichiers
  close_fs(fd, map, size);
  return 0;
}