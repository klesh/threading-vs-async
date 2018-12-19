#include <stdio.h>
#include <pthread.h>
#include <time.h>

#define THREAD_TOTAL 100
#define NSEC_PER_SEC 1000000000.0

int counter = 0;

void *incr(void *args) {
  for (int i = 0; i < 1000000; i++)
    counter++;
}

pthread_mutex_t lock;
void *incr_safe(void *args) {
  pthread_mutex_lock(&lock);
  for (int i = 0; i < 1000000; i++)
    counter++;
  pthread_mutex_unlock(&lock);
}

void main() {
  struct timespec start, end;
  clock_gettime(CLOCK_REALTIME, &start);
  counter = 0;
  pthread_t thread_ids[THREAD_TOTAL];
  for (int i = 0; i < THREAD_TOTAL; i++) {
    pthread_t thread_id;
#ifdef UNSAFE
    pthread_create(&thread_id, NULL, incr, NULL);
#else
    pthread_create(&thread_id, NULL, incr_safe, NULL);
#endif
    thread_ids[i] = thread_id;
  }
  for (int i = 0; i < THREAD_TOTAL; i++) {
    pthread_join(thread_ids[i], NULL);
  }
  clock_gettime(CLOCK_REALTIME, &end);
  double elapsed = end.tv_sec-start.tv_sec + (end.tv_nsec-start.tv_nsec)/NSEC_PER_SEC;
  printf("Counter: %d,  Elapsed: %f\n", counter, elapsed);
}
