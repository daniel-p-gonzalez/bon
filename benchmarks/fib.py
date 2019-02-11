# test run with:
# $ time python3 benchmarks/fib.py
# Calculating the 45th fibonacci number:
# 1134903170
#
# real    6m45.394s
# user    6m45.384s
# sys     0m0.004s

def fibonacci(n):
  if n < 2:
    return n
  else:
    return fibonacci(n-1) + fibonacci(n-2)

print("Calculating the 45th fibonacci number:")
print(fibonacci(45))
