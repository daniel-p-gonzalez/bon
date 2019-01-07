# Introduction
Bon is a programming language designed with simplicity, performance, and safety in mind.

## Simplicity

```ruby
def fibonacci(x)
    if x < 2
        x
    else
        fib(x-1) + fib(x-2)
    end
end
```

## Performance

Bon uses LLVM to generate efficient machine code, either JIT compiled, or pre-compiled to a binary.

Calculating the 45th fibonacci number using a simple recusive implementation (as above) in Bon, Ruby, and Python:

```bash
# Bon
$ daniel@daniel-laptop:~/bon$ time ./bon examples/fib.bon
1134903170

real    0m7.520s
user    0m7.492s
sys     0m0.016s

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
def factorial(x:float)->float
    match x
        0 => 1
        n => n * factorial(n - 1)
    end
end
```

It also allows for defining generic variant types, which helps prevent a whole class of errors by ensuring safe access to memory:

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

In addition, defining recusive data types such as lists or trees is straight forward:
```ocaml
type list<a>
    Empty
    List(a, list)
end

my_string_list = List("hello", Empty)
my_int_list = List(5, List(4, Empty))

head = match my_int_list
           List(hd, tl) => Some(hd)
           Empty => None
       end

type tree<a>
    Leaf
    Node(a, tree, tree)
end

my_tree = Node(5,
               Node(7,
                    Node(10,
                         Nil,
                         Nil),
                    Nil),
               Node(20,
                    Nil,
                    Node(18,
                         Nil,
                         Nil)))

def print_tree(a_tree)
    match a_tree
        Nil => print("Nil")
        Node(val, Nil, Nil) => print(val)
        Node(val, left, right) =>
            print(val)
            print_tree(left)
            print_tree(right)
        end
    end
end

```

Dedicated to the memory of Bonnie Gonzalez (May 26th 1957 - December 15th 2018).
