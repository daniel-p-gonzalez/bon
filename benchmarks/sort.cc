// test run with:
// time (clang++ -O3 --std=c++11 benchmarks/sort.cc -o sortcc && ./sortcc)
// Finished in 74ms (time for sort only)

// (time for full compile and run)
// real    0m0.457s
// user    0m0.387s
// sys     0m0.051s

#include <iostream>
#include <stdint.h>
#include <vector>
#include <chrono>

void swap(std::vector<int64_t> &xs, int64_t i, int64_t j) {
  auto tmp = xs[j];
  xs[j] = xs[i];
  xs[i] = tmp;
}

// identical algorithm to sort in Bon
void qsort(std::vector<int64_t> &xs, int64_t low, int64_t high) {
    int64_t mid = low + ((high - low) / 2);
    int64_t pivot = xs[mid];
    int64_t i = low;
    int64_t j = high;
    while (i <= j) {
        while (xs[i] < pivot) {
            i = i + 1;
        }
        while (xs[j] > pivot) {
            j = j - 1;
        }
        if (i <= j) {
            swap(xs, i, j);
            i = i + 1;
            j = j - 1;
        }
    }

    if (low < j) {
        qsort(xs, low, j);
    }

    if (i < high) {
        qsort(xs, i, high);
    }
}

void sort(std::vector<int64_t> &xs) {
    qsort(xs, 0, xs.size()-1);
}

// psuedo-random number generation
// intentional functionally identical to rand in Bon (same output, same seed)
int64_t s[2];

int64_t rotl(int64_t x, int64_t k) {
  return (x << k) | (x >> (64 - k));
}

// xoroshiro128+
int64_t xoroshiro128plus() {
  int64_t s0 = s[0];
  int64_t s1 = s[1];
  int64_t result = s0 + s1;

  s1 = s1 ^ s0;
  s[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16);
  s[1] = rotl(s1, 37);

  return result;
}

// xorshift64
int64_t simple_rand() {
  int64_t x = s[0];
  x ^= (x << 13);
  x ^= (x >> 7);
  x ^= (x << 17);
  s[0] = x;
  return x;
}

void seed_random(int64_t seed) {
  s[0] = seed;
  s[1] = seed;
  int64_t seed0 = seed + simple_rand();
  int64_t seed1 = seed + simple_rand();
  s[0] = seed0;
  s[1] = seed1;
}

int64_t get_time() {
  using namespace std::chrono;
  auto now = steady_clock::now();
  auto now_ms = time_point_cast<microseconds>(now).time_since_epoch();
  long value_ms = duration_cast<microseconds>(now_ms).count();
  return value_ms;
}

int main() {
  seed_random(172344);
  std::vector<int64_t> xs;
  for (size_t i = 0; i < 1000000; ++i) {
    xs.push_back(xoroshiro128plus());
  }

  auto start_time = get_time();
  sort(xs);
  auto end_time = get_time();
  auto total_time = end_time-start_time;
  std::cout << "Finished in " << (total_time/1000) << "ms" << std::endl;
  return 0;
}
