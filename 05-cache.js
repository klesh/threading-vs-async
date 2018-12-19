
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
