// test run with:
// $ time (clang++ -O3 --std=c++11 benchmarks/fib.cc -o fib && ./fib)
// Calculating the 45th fibonacci number:
// 1134903170
//
// real    0m6.545s
// user    0m6.486s
// sys     0m0.042s

#include <iostream>
#include <iomanip>

// Note: "double" version performs better on test machine (~5.9s)
int64_t fibonacci(int64_t n) {
  if (n < 2) {
    return n;
  }
  else {
    return fibonacci(n-1) + fibonacci(n-2);
  }
}

int main() {
  std::cout << "Calculating the 45th fibonacci number:" << std::endl;
  std::cout << fibonacci(45) << std::endl;

  return 0;
}
