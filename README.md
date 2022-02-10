# Introduction

Bon is a programming language designed around simplicity, performance, and safety.
If you just want a quick tour of the language, you can start [here](docs/ch01-03-quick-tour.md).


## Simplicity

In all aspects of its design, Bon aims to be succinct, yet readable.

Code written in Bon will look familiar to Python users:

```python
def fib(x):
    if x < 2:
        x
    else:
        fib(x-1) + fib(x-2)

print("Hello, World!")
print(fib(15) == 610)
```

However, the semantics are very different. It has a static type system with type inference (based on [hindley-milner](https://en.wikipedia.org/wiki/Hindley%E2%80%93Milner_type_system)), similar to languages such as Ocaml, Haskell, and Rust.

It allows you to write code that feels similar to object oriented programming languages, while using typeclasses instead of inheritance for polymorphism:

```python
typeclass Shape(T):
    def area(self: T) -> float

class Circle(Shape):
    Circle(radius: float)

    def area(self: Circle) -> float:
        pi = 4.0 * atan(1.0)
        return pi * self.radius ** 2.0

class Rectangle(Shape):
    Rectangle(width: float, height: float)

    def area(self: Rectangle) -> float:
        return self.width * self.height

# this can take instances of any type implementing the "Shape" typeclass
def print_area(shape):
    print(shape.area())

def main():
    circle = Circle(10.0)
    print_area(circle)

    rectangle = Rectangle(12.8, 0.25)
    print_area(rectangle)

main()
```

## Performance

Bon uses LLVM to generate efficient machine code, either JIT compiled, or pre-compiled to a binary.

This, along with zero-cost automatic memory mangement, keeps Bon in same performance ballpark as C++. For specific examples, see the [benchmarks](https://github.com/FBMachine/bon/tree/master/benchmarks).

## Safety

Performance doesn't have to come at the expense of safety, and safety doesn't have to come at the expense of simplicity. Bon has a powerful static type system which allows for simplifying code with type inference and pattern matching.

Learn more in Bon's [documentation](docs/ch00-01-contents.md).

## Current state

Bon is still very much in the prototype phase, and will likely have substantial changes over time.

## Why another programming language?

Bon is being designed as a programming language built around all of the things that make me enjoy programming, and it's dedicated to the memory of my mother. Her name was Bonnie, but everyone called her Bon, so that is where the name comes from.
More information is available on my Patreon page at https://www.patreon.com/d_p_gonz.
