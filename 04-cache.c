#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

#define THREAD_TOTAL 1000
#define NSEC_PER_SEC 1000000000.0

short is_loading = 0;
int cache = 0;
int counter = 0;
pthread_mutex_t lock;


int load_from_db() {
  sleep(1);
  counter++;
  return 123;
}

void* get_customer_detail_safe(void *args) {
  if (cache) {
    // good to go
    assert(cache == 123);
  } else if (is_loading) {
    // loop until cache is ready
    while (is_loading) {
      usleep(100);
    }
    // good to go
    assert(cache == 123);
  } else {
    printf("try to obtain the lock\n");
    // lock
#ifdef BLOCKING
    pthread_mutex_lock(&lock);
    if (!cache) {
      is_loading = 1;
      cache = load_from_db();
      is_loading = 0;
    }
    pthread_mutex_unlock(&lock);
#else
    if (pthread_mutex_trylock(&lock) == 0) {
      is_loading = 1;
      cache = load_from_db();
      is_loading = 0;
      pthread_mutex_unlock(&lock);
    } else {
      // loop until cache is ready
      while (is_loading) {
        usleep(100);
      }
    }
#endif
    // good to go
    assert(cache == 123);
  }
}

void* get_customer_detail_unsafe(void *args) {
  if (cache) {
    // good to go
  } else {
    cache = load_from_db();
    // good to go
  }
}

int main() {
  struct timespec start, end;
  clock_gettime(CLOCK_REALTIME, &start);
  pthread_t thread_ids[THREAD_TOTAL];
  for (int i = 0; i < THREAD_TOTAL; i++) {
    pthread_t thread_id;
#ifdef UNSAFE
    pthread_create(&thread_id, NULL, get_customer_detail_unsafe, NULL);
#else
    pthread_create(&thread_id, NULL, get_customer_detail_safe, NULL);
#endif
    thread_ids[i] = thread_id;
  }
  for (int i = 0; i < THREAD_TOTAL; i++) {
    pthread_join(thread_ids[i], NULL);
  }
  clock_gettime(CLOCK_REALTIME, &end);
  double elapsed = end.tv_sec-start.tv_sec + (end.tv_nsec-start.tv_nsec)/NSEC_PER_SEC;
  printf("Counter: %d  , Elapsed: %f\n", counter, elapsed);
}
