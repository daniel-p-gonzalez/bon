#include <iostream>
#include <iomanip>

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
