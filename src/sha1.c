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