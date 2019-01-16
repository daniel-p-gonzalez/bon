# Introduction

Bon is a programming language designed with simplicity, performance, and safety in mind.
Documentation can be viewed [here](docs/ch00-01-contents.md)

## Simplicity

In all aspects of its design, Bon aims to be both succinct, yet readable.

```ruby
def fibonacci(x)
    if x < 2
        x
    else
        fibonacci(x-1) + fibonacci(x-2)
    end
end
```

## Performance

Bon uses LLVM to generate efficient machine code, either JIT compiled, or pre-compiled to a binary.

Calculating the 45th fibonacci number using a simple recusive implementation (as above) in Bon, Ruby, and Python:

```bash
# Bon (full optimizations, includes JIT compile time)
$ daniel@daniel-laptop:~/bon$ time ./bon examples/fib.bon
1134903170

real    0m6.749s
user    0m6.725s
sys     0m0.005s

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

Performance doesn't have to come at the expense of safety, and safety doesn't have to come at the expense of simplicity. Bon has a powerful static type system which allows for simplifying code with type inference and pattern matching:

```ocaml
def factorial(x)
    match x
        0 => 1
        n => n * factorial(n - 1)
    end
end
```

Could also be written with explicit types:

```ocaml
def factorial(x:int)->int
    match x
        0 => 1
        n => n * factorial(n - 1)
    end
end
```

It also allows for defining generic variant types, which preventing a whole class of errors by ensuring safe access to memory:

```ocaml
type option<a>
    Some(a)
    None
end

x = Some(5.0)
match x
    Some(n) => print(n)
    None => print("None")
end

type result<a, b>
    Ok(a)
    Error(b)
end

response = call_that_may_fail()
match response
    Ok(data) => do_something(data)
    Error(err) => handle_error(err)
end
```

In addition, defining recusive data types such as lists or trees is straight forward. You can see more in the [documentation](docs/ch00-01-contents.md)

## Why another programming language?

Bon is being designed as a programming language built around all of the things that make me enjoy programming, and it's dedicated to the memory of my mother. Her name was Bonnie, but everyone called her Bon, so that is where the name comes from.
More information is available on my patreon page: https://www.patreon.com/d_p_gonz
