#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define NSEC_PER_SEC 1000000000.0
#define N 44

int fib(int n) {
  if (n < 2) return n;
  return fib(n - 1) + fib(n - 2);
}

void *hello(void *args) {
  for (int i = 0; i < 10; i++) {
    printf("hello\n");
    sleep(1);
  }
}

int main() {
  struct timespec start, end;
  clock_gettime(CLOCK_REALTIME, &start);
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, hello, NULL);

  printf("start calculating\n");
  fib(N);
  clock_gettime(CLOCK_REALTIME, &end);
  double elapsed = end.tv_sec-start.tv_sec + (end.tv_nsec-start.tv_nsec)/NSEC_PER_SEC;
  printf("n = %d, calculation took: %f\n", N, elapsed);
  pthread_join(thread_id, NULL);
  return 0;
}
