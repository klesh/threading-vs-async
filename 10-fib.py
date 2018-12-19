import time

def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

n = 37
start = time.time()
fib(n)
elapsed = time.time() - start
print('n = %d, calculation took: %f' % (n, elapsed))
