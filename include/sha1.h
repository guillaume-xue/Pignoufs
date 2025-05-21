#ifndef SHA1_H
#define SHA1_H

#include <stdint.h>
#include <pthread.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <string.h>

// Calcul SHA1 d'un bloc de données
void calcul_sha1(const void *data, size_t len, uint8_t *out);

void check_sha1(const void *data, size_t len, uint8_t *out);

typedef struct {
  void *data;
  size_t len;
  uint8_t sha1_ref[20];
  int block_num;
  int block_type;
} sha1_task_t;

#define SHA1_QUEUE_SIZE 256
typedef struct {
  sha1_task_t tasks[SHA1_QUEUE_SIZE];
  int head, tail;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int done; // Pour signaler la fin
} sha1_queue_t;

extern sha1_queue_t sha1_queue;

void *sha1_worker(void *arg);

#endif 