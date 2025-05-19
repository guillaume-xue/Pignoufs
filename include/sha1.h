#ifndef SHA1_H
#define SHA1_H

#include <stdint.h>
#include <pthread.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <string.h>

// Calcul SHA1 d'un bloc de données
static void calcul_sha1(const void *data, size_t len, uint8_t *out)
{
  EVP_MD_CTX *ctx = EVP_MD_CTX_new(); // Crée un contexte pour le calcul
  if (!ctx)
  {
    perror("Erreur : Impossible de créer le contexte EVP");
    return;
  }
  if (EVP_DigestInit_ex(ctx, EVP_sha1(), NULL) != 1)
  {
    perror("Erreur : EVP_DigestInit_ex a échoué");
    EVP_MD_CTX_free(ctx);
    return;
  }
  if (EVP_DigestUpdate(ctx, data, len) != 1)
  {
    perror("Erreur : EVP_DigestUpdate a échoué");
    EVP_MD_CTX_free(ctx);
    return;
  }
  if (EVP_DigestFinal_ex(ctx, out, NULL) != 1)
  {
    perror("Erreur : EVP_DigestFinal_ex a échoué");
    EVP_MD_CTX_free(ctx);
    return;
  }
  EVP_MD_CTX_free(ctx);
}

static void check_sha1(const void *data, size_t len, uint8_t *out)
{
  uint8_t sha1[20];
  calcul_sha1(data, len, sha1);
  if (memcmp(sha1, out, 20) != 0)
  {
    fprintf(stderr, "Erreur: SHA1 ne correspond pas\n");
    printf("SHA1 attendu : ");
    for (int i = 0; i < 20; i++)
      printf("%02x", out[i]);
    printf("\nSHA1 calculé : ");
    for (int i = 0; i < 20; i++)
      printf("%02x", sha1[i]);
    printf("\n");
    // exit(EXIT_FAILURE);
  }
}

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