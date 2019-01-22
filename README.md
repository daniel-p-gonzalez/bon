# Introduction

Bon is a programming language designed with simplicity, performance, and safety in mind.
If you just want a quick tour of the language, you can start [here](docs/ch01-03-quick-tour.md).


## Simplicity

In all aspects of its design, Bon aims to be succinct, yet readable.

Code written in Bon tends to look similar to a scripting language such as Ruby or Python:

```ruby
def fib(x)
    if x < 2
        x
    else
        fib(x-1) + fib(x-2)
    end
end

print("Hello, World!")
print(fib(15) == 610)
```

## Performance

Bon uses LLVM to generate efficient machine code, either JIT compiled, or pre-compiled to a binary.

Calculating the 45th fibonacci number using a simple recusive implementation (as above) in Bon, C++, Ruby, and Python:

```bash
# Bon (time for compile and run)
$ daniel@daniel-laptop:~/bon$ time ./bon examples/fib.bon
1134903170

real    0m6.749s
user    0m6.725s
sys     0m0.005s

# C++ (time for compile and run)
$ daniel@daniel-laptop:~/bon$ time (clang++ fib.cc -o fib -O3 && ./fib)
1134903170

real    0m6.071s
user    0m5.996s
sys     0m0.054s

# Ruby
daniel@daniel-laptop:~/bon$ time ruby fib.rb
1134903170

real    2m16.625s
user    2m16.578s
sys     0m0.020s

# Python
daniel@daniel-laptop:~/bon$ time python fib.py
1134903170

real    4m42.735s
user    4m42.724s
sys     0m0.012s
```

## Safety

Performance doesn't have to come at the expense of safety, and safety doesn't have to come at the expense of simplicity. Bon has a powerful static type system which allows for simplifying code with type inference and pattern matching. In addition, defining recusive data types such as lists or trees is straight forward.

Learn more in the [documentation](docs/ch00-01-contents.md)

## Why another programming language?

Bon is being designed as a programming language built around all of the things that make me enjoy programming, and it's dedicated to the memory of my mother. Her name was Bonnie, but everyone called her Bon, so that is where the name comes from.
More information is available on my patreon page: https://www.patreon.com/d_p_gonz
