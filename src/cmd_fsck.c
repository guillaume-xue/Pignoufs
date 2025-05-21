#include "../include/structures.h"
#include "../include/utilitaires.h"

#define NB_THREADS 6

int cmd_fsck(const char *fsname)
{
  sha1_queue_t queue = {.head = 0, .tail = 0, .done = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
  pthread_t threads[NB_THREADS];
  for (int i = 0; i < NB_THREADS; i++)
    pthread_create(&threads[i], NULL, sha1_worker, &queue);

  uint8_t *map;
  size_t size;
  int fd = open_fs(fsname, &map, &size);
  if (fd < 0)
    return print_error("Erreur lors de l'ouverture du système de fichiers");

  int32_t nb1, nbi, nba, nbb;
  get_conteneur_data(map, &nb1, &nbi, &nba, &nbb);
  struct pignoufs *sb = (struct pignoufs *)map;

  if (strncmp(sb->magic, "pignoufs", 8) != 0)
  {
    print_error("Erreur: le conteneur n'est pas un système de fichiers Pignoufs");
    close_fs(fd, map, size);
    return 1;
  }

  for (int32_t i = 1; i < nbb; i++)
  {
    struct bitmap_block *bb = (struct bitmap_block *)((uint8_t *)map + (int64_t)(i) * 4096);
    if (i <= nb1)
    {
      if (bb->type != TO_LE32(2))
      {
        print_error("Erreur: bloc de bitmap a un type incorrect");
        close_fs(fd, map, size);
        return 1;
      }
    }
    else if (i <= nb1 + nbi)
    {
      if (bb->type != TO_LE32(3))
      {
        print_error("Erreur: bloc d'inode a un type incorrect");
        close_fs(fd, map, size);
        return 1;
      }
      struct inode *in = (struct inode *)bb;
      if (in->flags & 1) // Si l'inode est allouée
      {
        if (!check_bitmap_bit(map, i))
        {
          print_error("Erreur: inode a un bloc de données non alloué");
          close_fs(fd, map, size);
          return 1;
        }
        in->flags &= ~(1 << 3);
        in->flags &= ~(1 << 4);
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
        if ((int32_t)FROM_LE32(in->double_indirect_block) >= 0)
        {
          struct address_block *dbl = get_address_block(map, FROM_LE32(in->double_indirect_block));
          if (FROM_LE32(dbl->type) == 6)
          {
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
      if (bb->type < TO_LE32(4) || bb->type > TO_LE32(7))
      {
        print_error("Erreur: bloc de données a un type incorrect");
        close_fs(fd, map, size);
        return 1;
      }
    }

    unlock_block(fd, i * 4096);

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

  // Signaler la fin
  pthread_mutex_lock(&queue.mutex);
  queue.done = 1;
  pthread_cond_broadcast(&queue.cond);
  pthread_mutex_unlock(&queue.mutex);

  // Attendre la fin des threads
  for (int i = 0; i < NB_THREADS; i++)
    pthread_join(threads[i], NULL);

  close_fs(fd, map, size);
  return 0;
}