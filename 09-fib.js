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

