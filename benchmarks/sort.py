import time
import random

def qsort(xs, low, high):
  mid = low + ((high - low) // 2)
  pivot = xs[mid]
  i = low
  j = high
  while i <= j:
    while xs[i] < pivot:
      i = i + 1
    while xs[j] > pivot:
      j = j - 1
    if i <= j:
      tmp = xs[i]
      xs[i] = xs[j]
      xs[j] = tmp
      i = i + 1
      j = j - 1
  if low < j:
    qsort(xs, low, j)
  if i < high:
    qsort(xs, i, high)

def sort(xs):
  qsort(xs, 0, len(xs)-1)

def main():
  batch_size = 1000000

  v = []
  rng = random.SystemRandom()
  for i in range(batch_size):
    v += [rng.randint(-2**63, 2**63-1)]

  start_time = time.time()
  sort(v)
  total_time = time.time() - start_time
  print("Finished in " + str(int(total_time*1000)) + "ms")

main()
