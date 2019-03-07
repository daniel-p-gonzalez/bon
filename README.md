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

This, along with zero-cost automatic memory mangement, keeps Bon in same performance ballpark as C++. For specific examples, see the [benchmarks](https://github.com/FBMachine/bon/tree/master/benchmarks).

## Safety

Performance doesn't have to come at the expense of safety, and safety doesn't have to come at the expense of simplicity. Bon has a powerful static type system which allows for simplifying code with type inference and pattern matching. In addition, Bon allows you to easily define custom behaviors for scenarios such as out-of-bounds memory accesses per type.

Learn more in Bon's [documentation](docs/ch00-01-contents.md).

## Why another programming language?

Bon is being designed as a programming language built around all of the things that make me enjoy programming, and it's dedicated to the memory of my mother. Her name was Bonnie, but everyone called her Bon, so that is where the name comes from.
More information is available on my Patreon page at https://www.patreon.com/d_p_gonz.
