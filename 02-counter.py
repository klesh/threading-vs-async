import asyncio
import time
import sys
from threading import Thread, Lock

THREAD_TOTAL = 100
counter = 0

def incr():
    global counter
    for i in range(1000000):
        counter += 1

async def incr_async():
    global counter
    for i in range(1000000):
        counter += 1

lock = Lock()
def incr_safe():
    global counter, lock
    lock.acquire()
    for i in range(1000000):
        counter += 1
    lock.release()

def count(target):
    threads = []
    for i in range(THREAD_TOTAL):
        thread = Thread(target=target)
        threads.append(thread)

    for thread in threads:
        thread.start()

    for thread in threads:
        thread.join()

async def count_async():
    start = time.time()
    coros = []
    for _ in range(THREAD_TOTAL):
        coros.append(incr_async())
    await asyncio.gather(*coros);
    end = time.time()
    print('async  elapsed: %f' % (end-start))

start = time.time()
if len(sys.argv) > 1:
    if sys.argv[1] == 'UNSAFE':
        count(incr)
    elif sys.argv[1] == 'ASYNC':
        asyncio.run(count_async())
else:
    count(incr_safe)
end = time.time()
print('Counter: %d,  Elapsed: %f' % (counter, end-start))
