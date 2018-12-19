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
