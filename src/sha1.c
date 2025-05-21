#include "../include/sha1.h"

void *sha1_worker(void *arg) {
  sha1_queue_t *queue = (sha1_queue_t *)arg;
  while (1) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->head == queue->tail && !queue->done)
        pthread_cond_wait(&queue->cond, &queue->mutex);
    if (queue->head == queue->tail && queue->done) {
        pthread_mutex_unlock(&queue->mutex);
        break;
    }
    sha1_task_t task = queue->tasks[queue->head];
    queue->head = (queue->head + 1) % SHA1_QUEUE_SIZE;
    pthread_mutex_unlock(&queue->mutex);

    check_sha1(task.data, task.len, task.sha1_ref);
  }
  return NULL;
}

void check_sha1(const void *data, size_t len, uint8_t *out)
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

// Calcul SHA1 d'un bloc de données
void calcul_sha1(const void *data, size_t len, uint8_t *out)
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