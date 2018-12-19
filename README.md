---
title: threading vs async
date: 2018-12-07 16:00:29
categories:
  - Programming
---

# 说明
本文旨在探讨__单一线程中线程模型和异步模型在完成类似的功能下的区别__, 而由于进程管理(如gunicorn/pm2等)和架构上的优化(如读写分离等)属于进程外的范围, 对两都是适用的, 因此不在讨论之列. 请知悉.

# 多任务
我们常常会遇到一种情况: 我们开发的系统除了它的首要任务外,还常常伴随着有一些周期性的任务需要随之运行. 比如说, 到午夜的时候清空一下临时文件, 或者定时地发心跳包等等. 这些次要任务与主要任务之间相互不能阻塞, 如果它们之间互不相关独立运行的话, 解决起来也很简单:

1. 跑一个独立的脚本或进程定时执行(不作深入讨论)
2. 起一个相对独立的线程, 循环睡眠执行
3. 异步事件定时执行

但有的时候, 情况可能会变得复杂. 比如说,需要在主任务里面控制这些定时任务具体是不是执行,或说动态地去修改执行间隔等. 对于独立的进程,可能就需要加一些机制进行去实现进程通信. 特别如果这些次要任务数量较多的话, 进程的通信和管理将变得很复杂. 这时,线程管理起来相对就方便一些, 通信也相对容易, 然而代价是需要考虑线程安全的问题. 线程安全又是一个不好处理的问题

# 共享计数器篇
请看以下基于线程模型的实现代码(c实现)
```c
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
  struct timespec start, end; // clock_t 计时不准，换timespec
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
```
编译得到不安全和安全的可执行版本:
```fish
$ gcc -o bin/counter-unsafe -lpthread -DUNSAFE 01-counter.c
$ gcc -o bin/counter-safe -lpthread 01-counter.c
```
输出参考：
counter-unsafe
```
Counter: 6296382  Elapsed: 0.323539
```
counter-safe
```
Counter: 100000000  Elapsed: 0.240659
```

以上的命令执行多次，大概可以看得出来:

  1. 不加锁的情况下，速度稍慢，数值不准确
  2. 加锁的情况下，速度稍快，数值准确

不安全版本的数值错误，是因为语句`counter += 1`会被拆成 __读值，做加法，存回__ 三条指令(也可能不止)，若操作系统在这几步中间切到别的线程上，必然会导致不符合预期的结果。

虽然加锁版本比不加锁还要快这一点很违反直觉，但这个是事实。事实上, 锁竞争的并没有相象中那么激烈（这个后面还会有例子说明），安全版本之所以会更快，是因为它在获得了锁之后，进行了全部的操作再释放锁给下一个线程。也即它保证了同一时间只有一个线程在执行，避免了很多线程的切换。在线程切换的过程中，操作系统需要先将当前上下文压栈，切回来时再弹栈。单一切换的损耗可能比锁竞争小, 但在数量差别具大的情况下切换带来的开销要比锁竞争明显得多。这个事实，是GIL的理论基础，python社区周期性地会出现 去除GIL 的动议和尝试，至今没人成功, 这里有[一篇 python 的 wiki](https://wiki.python.org/moin/GlobalInterpreterLock)提到相关的问题(注意看Speed那一项)

来看看异步模型做类似的事情是怎么弄的(node实现):
```node
PROMISE_TOTAL = 100

let counter = 0;

async function incr() {
  for (let i = 0; i < 1000000; i++)
    counter++;
}

async function count() {
  counter = 0;
  const ps = [];
  for (let i = 0; i < PROMISE_TOTAL; i++) {
    ps.push(incr());
  }
  await Promise.all(ps);
}

(async () => {
  const start = process.hrtime();
  await count();
  const elapsed = process.hrtime(start);
  const seconds = elapsed[0] + elapsed[1]/1000000000;
  console.log('Counter: %d,  Elapsed: %d', counter, seconds);
})();
```
输出参考：
```
Counter: 100000000,  Elapsed: 0.203944253
```
居然比c还快那么一点点。

附上基于 python 的线程和异步实现:
```python
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
    # 若没有这个for，在py3中是可以得到正确结果的
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
    coros = []
    for _ in range(THREAD_TOTAL):
        coros.append(incr_async())
    await asyncio.gather(*coros);

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
```
通过执行以下三个命令，我们可以得到安全，安全非和异步的计数结果和耗时：
```
python 02-counter.py
python 02-counter.py UNSAFE
python 02-counter.py ASYNC
```
大致分别为：
```
Counter: 100000000,  Elapsed: 6.041980
Counter: 35023466,  Elapsed: 6.367411
Counter: 100000000,  Elapsed: 5.56256
```
ps:
  1. 这里要特别提一下在py3中, GIL的释放策略由原来py2的每N条"指令"释放一次, 变成了每隔一定时间(默认5ms)释放一次. 此时若累加操作完成太快会导致线程看起来似乎是安全的, 因此测试中特别地对累加操作连续执行百万次.
  2. python本身的执行效率不高, 不难推测出大量的时间花费在了循环累加上, 把这部时间去掉的话, 线程模型和异步模型的执行时间比例会大幅上升.

回到正题，以上几段代码。都是在模拟有多个并发请求，对同一共享变量的进行读写的情况。
相对于多线程, 异步模型完成同样的事情， 不但效率更好，写起来也更简练，需要操心的事情更少。对于共享变量，我们可以在任意的地方，毫无顾忌地任意读写

# 缓存加载篇
我们用一个例子来说明一下异步编程在功能上给我们带来了哪些便利. 假定我们的系统用户量挺大, 有些数据加载要花较长的时间, 很自然我们会使用到缓存. 缓存是一种看起来简单用起来其实挺麻烦的东西. 就比如说冷加载吧, 假定现在系统刚初始化或者缓存刚被清空. 这时候当用户访问到某个数据时, 我们就把它加载到缓存里再返回, 后续用户访问就直接从缓存读出返回. 但我们知道缓存加载需要一定时间, 在开始加载到加载完成的这一段时间内, 有其它用户也在请求呢? 或者更极端点, 这个时候是访问高峰, 缓存过期了. 此时有几百个用户同时在请求呢? 理想处理方式是, 保证整个过程中只有一个人去加载缓存, 其他人等到它加载完了直接使用缓存就行了. 对应到线程模型, 那即是保证只有一个线程在加载, 其它线程都在等待. 这是非常典型的线程同步问题
```c
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
```
通过以下命令编译分别得到非安全，安全(使用trylock)和安全(使用阻塞锁)三个版本：
```
gcc -o bin/cache-unsafe -lpthread -DUNSAFE 04-cache.c
gcc -o bin/cache-safe -lpthread 04-cache.c
gcc -o bin/cache-safe-blocking -lpthread -DBLOCKING 04-cache.c
```
非安全的参考输出：
```
Counter: 996  , Elapsed: 1.018113
```
安全的trylock:
```
try to obtain the lock
try to obtain the lock
try to obtain the lock
try to obtain the lock
try to obtain the lock
Counter: 1  , Elapsed: 1.008818
```
安全的阻塞锁：
```
try to obtain the lock
try to obtain the lock
Counter: 1  , Elapsed: 1.008515
```
其中，安全的两个版本可以看出，锁竞争的情况并不严重。因此，使用trylock和lock不会有明显的差异

回到正题，在非安全的版本中，缓存被加载了996次！这若发现在生产环境，很容易由于某些节点瞬时负载过大，从而引发连锁反应。因此，保证某时耗时很长的操作在同一时间只被加载一次还是很有必要的。
我们来看看异步模型下可以怎么解决:
```
let counter = 0;

function loadFromDb() {
  return new Promise(resolve => {
    counter++;
    setTimeout(() => resolve(123), 1000);
  });
}

let loading = null;
let cache = null;
async function getCustomerDetailSafe() {
  if (cache) {
    // good to go
  } else {
    loading = loading || loadFromDb().then(() => {
      loading = null;
    });
    cache = await loading;
    // good to go
  }
}

async function getCustomerDetailUnsafe() {
  if (cache) {
    // good to go
  } else {
    cache = await loadFromDb();
    // good to go
  }
}

(async() => {
  const start = process.hrtime();
  const requests = [];
  for (let i = 0; i < 1000; i++) {
    requests.push(getCustomerDetailSafe());
    //requests.push(getCustomerDetailUnsafe());
  }

  await Promise.all(requests);
  const elapsed = process.hrtime(start);
  const seconds = elapsed[0] + elapsed[1]/1000000000;
  console.log('Counter: %d,  Elapsed: %d', counter, seconds);
})();
```
请允许我只贴安全版本的参考结果：
```
Counter: 1,  Elapsed: 1.005952907
```
经过N次重复实验，结果总是会比多线程的快一点点
注意，这个结果是很不得了的。因为c的执行速度比node快很多。有兴趣的同学可以试一下附带的三个指数级(n每加1，计算量翻一番) fib 实现，测一下它们间的速度差别。
异步模型能做到这一点，是因为所有你编写的代码，都是在一个线程里面执行的。也就不存在线程安全的问题，上面的loading会且只会被赋值一次。
反观线程模型，即使在这个简单的, 只有一个锁的情况, 在实现时也要小心翼翼. 若是情况再复杂些, 再多几个锁还需要操心死锁的问题. 更别提多人协作时情况会变得多么不可控了.
总的来说, 异步模型对于以往一些只能由线程来完成的功能非常地方便好用, 而且由于少了线程切换和锁竞争的开销, 并且速度往往有肉眼可见的提高

ps: 以上实现的是一种单一进程下响应式的缓存策略, 可以在进程内, 进程外, 任意的时间, 任意的方式清空缓存, 所有在缓存清空后访问的用户都能看到最新的数据. 我见过一些系统, 采用独立进程定时更新缓存的策略, 用户在使用的时候往往需要等待缓存更新, 当然也不能随便地通过管理界面去清缓存了. 这些都只是策略问题, 与编程模型无关.


# 线程的Flask 与 异步aiohttp

以下是参照 Flask 官方的代码：
```python
import time
import redis
from flask import Flask
from threading import Lock
import logging
log = logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)

app = Flask(__name__)

lock = Lock()
counter = 1

@app.route('/')
def index():
    global counter
    lock.acquire()
    counter += 1
    lock.release()
    return str(counter)

@app.route('/slow')
def slow():
    time.sleep(1)
    return 'ok'
```
我们先来验证一下，Flask是多线程的，注意上面`/slow`，每个请求要花1秒。那我们并发的请求100个应该要等100s
```
$ ab -n 100 -c 100 http://localhost:5000/slow
```
截取部分结果：
```
Time taken for tests:   2.217 seconds
```
总共花了2.217秒，说明请求之间不会相互阻塞，可以确认Flask当前是运行在多线程的状态下。

aiohttp 也是依照官方文档给的例子稍微调整：
```python
from aiohttp import web

counter = 0

async def index(request):
    global counter
    counter += 1
    return web.Response(text=str(counter))


app = web.Application()
app.add_routes([web.get('/', index)])

web.run_app(app, port=5000)
```
好，功能一样，我们来看看输出结果
Flask
```
Concurrency Level:      300
Time taken for tests:   0.687 seconds
Complete requests:      1000
Failed requests:        0
Total transferred:      157000 bytes
HTML transferred:       4000 bytes
Requests per second:    1456.00 [#/sec] (mean)
Time per request:       206.044 [ms] (mean)
Time per request:       0.687 [ms] (mean, across all concurrent requests)
Transfer rate:          223.23 [Kbytes/sec] received
```
aiohttp
```
Concurrency Level:      300
Time taken for tests:   0.269 seconds
Complete requests:      1000
Failed requests:        0
Total transferred:      154000 bytes
HTML transferred:       4000 bytes
Requests per second:    3717.90 [#/sec] (mean)
Time per request:       80.691 [ms] (mean)
Time per request:       0.269 [ms] (mean, across all concurrent requests)
Transfer rate:          559.14 [Kbytes/sec] received
```
也许Flask不是线程模型的最佳实现，但aiohttp也一样不是最优的(比如sanic)。也许Flask还有许多优化的空间，在上生产环境还需要一阵敲打才能发挥出实力。但真这样话,那也只能是缺点吧? 要知道 aiohttp 上生产环境并不需要特别的配置，那么问题来了：
  1. 在单进程的情况下可能配置出一倍效率来吗？
  2. 开发环境与生产环境不一致管理起来麻烦吗？
  3. 多线程程序好调吗？

# 局限性
一件东西有它的长处, 必然有它的短处, 毕竟异步模型又不是马克思主义. 我们来讲讲它的局限性. 也许有的同学可能已经注意到, 我刚才讲的例子都是属于I/O密集性. 基本上都没有什么运算. 如果你的系统是CPU密集型的话, 异步模型就不是那么适用了. 我们来看一下指数级的斐波那契数列
```c
start calculating
n = 44, calculation took 7.646799437
hello
hello
hello

/* 输出大概如下:
start calculating
hello
hello
hello
hello
hello
n = 44, calculation took: 4.572308
hello
*/
```
node 版
```node
const fib = (n) => {
  if (n < 2) return n;
  return fib(n - 1) + fib(n - 2);
};

setInterval(() => {
  console.log('hello');
}, 1000);

(async() => {
  const n = 44;
  const start = process.hrtime();
  console.log('start calculating');
  fib(n);
  const elapsed = process.hrtime(start);
  const seconds = elapsed[0] + elapsed[1]/1000000000;
  console.log('n = %d, calculation took %d', n, seconds);
})();


/* 输出大概如下:
start calculating
n = 44, calculation took 7.646799437
hello
hello
hello
...
*/
```
异步的事件循环机实际上相当于一个队列, 在当前代码段(请想像成以await关键字为分隔的分段)未执行完之前, 后面的代码段只能等待. 而线程由操作系统调度, 独立性是有保障的. 所以异步模型并不适合用来做有大量计算的事情. 当然这样的问题也好处理,只要将计算密集的逻辑分离出去(采用线程,进程,网络)把它变成I/O就行了.
异步还有另外一个问题是由于没有多线程，导致在多核机器上只能通过多进程来充分利用CPU。不过，新版的 node 已经开始支持 WorkerThread 了。至于python线程, 呵呵!

# 结论
通过以上的对比, 可以看出来对于并发任务来讲, 异步模型实现更为方便直观, 效率上也更优秀. 考虑到绝大多数系统都不是CPU密集型的, 掌握异步模型非常重要. 而在使用时也需要注意它的执行特点, 它更适合用于少量计算, 大量I/O, 大量吞吐的场景. 若中间有大量的计算, 将会阻塞整个进程, 需要注意将大量计算分离出去.
