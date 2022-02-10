def fibonacci(n):
  if n < 2:
    return n
  else:
    return fibonacci(n-1) + fibonacci(n-2)

print("Calculating the 45th fibonacci number:")
print(fibonacci(45))
