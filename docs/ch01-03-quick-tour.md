## A Quick Tour

To get a feel for the language, we're going to go over some of the core concepts in Bon. We won't get too bogged down in details, instead taking only a brief look at some of the features.

### Functions

To define a function in Bon, you use the "def" keyword. The [off-side rule](https://en.wikipedia.org/wiki/Off-side_rule) is used to delimit blocks, resulting in a syntax very similar to Python:

```python
def square(x):
    return x * x
```

Function calls consist of the function name, followed by a list of arguments in parentheses:

```python
square(19.57)
```

You can introduce local variables into functions with the assignment operator:

```python
def noisy_square(x):
    y = x * x
    print(str(x) ++ " squared is " ++ str(y))
    return y
```

Note that `++` is the concatenation operator in Bon, which works on both strings and lists.

### Control Flow

If-then-else is an expression in Bon, and so it can also be used as the last expression in a function:

```python
def fibonacci(x):
    if x < 2:
        x
    else:
        fibonacci(x-1) + fibonacci(x-2)
```

You'll probably notice the lack of an explicit "return". This is because functions in Bon always return the value of the last expression.

Another form of control flow in Bon is the match expression, which can be used to branch based on matching a value with a pattern:

```rust
def factorial(x):
    match x:
        0 => 1
        n => n * factorial(n - 1)
```

Let's unpack the match expression by looking the patterns (also called match "cases"):

```python
0 => 1
```

This is our first match case, and it means: if `x` is `0`, the match expression returns (evaluates to) `1`.

The next match case is a variable `n`:

```python
n => n * factorial(n - 1)
```

A variable will always match to the input value (or a part of the value for more complex types). In this example, using pattern matching doesn't save us a whole lot over an if-else expression. However, pattern matching comes alive when used in combination with variants, which we'll return to after a quick introduction to Bon's type system.

### Type Checking and Type Inference

You'll notice in all of the above examples that we didn't specify any types for variables or in function definitions. However, this does not mean Bon is a dynamically typed language. It has a type system based on [Hindley-Milner](https://en.wikipedia.org/wiki/Hindley–Milner_type_system), similar to languages like Ocaml and Haskell.

You don't have to learn all the details up front in order to be able to take advantage of the type system - in fact you already have if you've followed along with the examples.

The square function that we wrote above could have also explicitly specified types:

```python
def square(x: float) -> float:
    return x * x
```

However, since types can be automatically inferred by their use, it's convenient to omit them. Also, if we specify the type as `float` in this case, we can no longer also use the function with integers.

### Generic function parameters

Bon supports writing generic functions by allowing function parameter types to remain unbound until code needs to be generated for a function call. Here's an example:

```python
def identity(x):
    return x

def add(x, y):
    z = x + y
    return z

print("Calling 'identity' with a number: " ++ str(identity(1)))
print("Calling 'identity' with a string: " ++ identity("hello!"))

print("Calling 'add' with integers: " ++ str(add(5, 10)))
print("Calling 'add' with floats: " ++ str(add(19.57, 20.18)))
```

The output:

```bash
Calling 'identity' with a number: 1
Calling 'identity' with a string: hello!
Calling 'add' with integers: 15
Calling 'add' with floats: 39.75
```

### Types and Typeclasses

To define a type in Bon, we use the `class` keyword, and specify a __constructor__ for the type:

```python
class Point2D:
    Point2D(x: float, y: float)
```

Here's how we can construct an object of this type, and access its named fields:

```python
p = Point2D(2.0, 1.5)
print(p.x)
```

Bon supports polymorphic types for function and operator overloading using __typeclasses__. As an example, we're going to create a simple typeclass which will require classes implementing the typeclass to define a single function `norm`:

```python
typeclass Norm(T):
    def norm(v: T) -> float
```

To implement norm for our 2d point type above, we simply have to add the implementation:

```python
impl Norm(Point2D):
  def norm(v: Point2D) -> float:
    return sqrt(v.x * v.x + v.y * v.y)
```

Note that we didn't have to implement this as part of the type's original definition, allowing us to extend it after the fact.

Let's look at a more complete example with an additional 3d point type.

Filename: typeclass.bon

```python
typeclass Norm(T):
  def norm(v:T) -> float

class Point2D(Norm):
  Point2D(x: float, y: float)

  def norm(v: Point2D) -> float:
    sqrt(v.x * v.x + v.y * v.y)

class Point3D(Norm):
  Point3D(x: float, y: float, z: float)

  def norm(v: Point3D) -> float:
    sqrt(v.x * v.x + v.y * v.y + v.z * v.z)

def main():
  print("Test of typeclasses:")
  p = Point2D(2.0, 1.5)
  print(p.norm())
  p2 = Point3D(2.0, 1.5, 3.0)
  print(p2.norm())

main()
```

In this example, we're explicitly specifying `Norm` as a typeclass which the point classes implement, and including the definition of the norm function inside the class definition. Another thing in the above example which we haven't discussed is the use of the syntactic sugar `p.norm()`. This is identical to calling the function as norm(p), and works for passing the first argument of any function (not just typeclass methods).

Typeclasses also allow us to overload more than one parameter:

Filename: multiple_dispatch.bon

```python
# define a simple typeclass "MultiDispatch" with a single method "foo"
#  which takes two parameters with generic types
typeclass MultiDispatch(T1, T2):
  def foo(a: T1, b: T2) -> ()

# implement for a:int, b:float
impl MultiDispatch(int, float):
  def foo(a: int, b: float) -> ():
    print("Got an int: " ++ str(a) ++ ", and a float: " ++ str(b))

# implement for a:string, b:int
impl MultiDispatch(string, int):
  def foo(a: string, b: int) -> ():
    print("Got an string: " ++ str(a) ++ ", and an int: " ++ str(b))

# this can be called with any pair of types for which we implemented
# method foo of the MultiDispatch typeclass
def some_function(a, b):
  foo(a, b)

def main() -> ():
  some_function(5, 10.0)
  some_function("Hello, World!", 10)

  return ()

main()
```

The output:
```bash
Got an int: 5, and a float: 10
Got an string: Hello, World!, and an int: 10
```

Here are a few examples using typeclasses available in the stdlib:

```python
impl Object(vec):
  # called when freeing memory allocated on the heap:
  def delete(self: vec) -> ():
    print("deleting vector with size: " ++
          str(self.size) ++
          " and capacity: " ++ str(self.capacity))

# implementing equality (==) operator for a custom class:
class Color(Eq):
  Cyan
  Magenta
  Yellow
  Black

  def operator==(a: Color, b: Color) -> bool:
    match (a,b):
      (Cyan, Cyan) => true
      (Magenta, Magenta) => true
      (Yellow, Yellow) => true
      (Black, Black) => true
      _ => false
```

### Variants

The power of pattern matching becomes apparent when we would like to constrain our program logic to respect the uncertain nature of our program state. As an example, if the user of our program requests a non-existent file, we have a few options. Some of the more common approaches are returning a null pointer, or an error code.

To handle situations like this in Bon, we can use either an "option" type, or the "result" type.

The option type is defined as:

```ocaml
class Option(a):
    Some(a)
    None
```

Notice how unlike the examples in the previous section, there are now two constructors: `Some` and `None`. We call types with multiple constructors __variants__ (or alternatively, __sum types__). We can construct instances of these types similarly:

```ocaml
x = Some(5)
y = None
```

Now in order to know which constructor we used to construct our `Option` instance, or to access the value within our `Some` instance, we need to use pattern matching:

```rust
match x:
    Some(n) => print("Found: " + to_string(n))
    None => print("Found: None")
```

Using this `Option` type allows us to ask the compiler to enforce safe access to data, rather than relying on proper use of conventions (for example remembering to check for null pointers before dereferencing them). The usefulness of variants extends beyond simple types like `Option` however, which we'll look at in a future chapter on algebraic data types.

### Vectors

You can define a vector (dynamic array) like so:

```python
x = [1,4,5,1]
```

Here are some examples of functionality available to vectors:
```python
# indexing an element
x[i]
# getting the current length of the vector
x.len()
# swapping two elements of the vector
x.swap(0,2)
# guarded indexing
y = x.at(5)
match y:
  Some(a) => print(a)
  None => print("Index out of bounds!")
```

As Bon is still in early development, the only loop construct currently available is the while loop, which can be used to iterate over the vector:

```python
i = 0
while i < v.len():
  print(v[i])
  i = i + 1
```

As another example, here is a simple implementation of insertion sort:

```python
def insertion_sort(xs):
    i = 1
    while i < xs.len():
        j = i
        while j > 0 and xs[j-1] > xs[j]:
            xs.swap(j, j-1)
            j = j - 1
        i = i + 1
    return ()
```

### Wrap Up

Finally, let's look at an example that uses some of the things we've learned up to this point.
It's a simple program which prints out a description of a programming language given its capabilities. Try not to take it too seriously!

Filename: languages.bon

```python
# strong or weak type system?
class type_safety:
  Strong
  Weak

# types checked at compile type, or runtime?
class type_checking:
  Static
  Dynamic

class type_system:
  TypeSystem(type_safety, type_checking)

class language:
  Language(name: string,
           typing: type_system,
           type_infer: bool,
           pattern_match: bool,
           polymorphism_support: string)

# generate description string by pattern matching its capabilities
def describe_language(lang):
  desc = lang.name ++ " is a programming language with:\n"

  desc = match lang.typing:
    TypeSystem(Strong, Static) => desc ++ "  - Strong, static type system\n"
    TypeSystem(Strong, Dynamic) => desc ++ "  - Strong, dynamic type system\n"
    TypeSystem(Weak, Static) => desc ++ "  - Weak, static type system\n"
    TypeSystem(Weak, Dynamic) => desc ++ "  - Weak, dynamic type system\n"

  desc = match lang.type_infer:
    true => desc ++ "  - Type Inference\n"
    false => desc

  desc = match lang.pattern_match:
    true => desc ++ "  - Pattern Matching\n"
    false => desc

  desc = desc ++ "  - Support for polymorphism through " ++ lang.polymorphism_support
  return desc ++ "\n"

# implement printing and conversion to string for language type
impl Print(language):
  def print(lang: language) -> ():
    print(describe_language(lang))

  def to_string(lang: language) -> string:
    describe_language(lang)

def main():
  bon = Language("Bon", TypeSystem(Strong, Static), true, true, "typeclasses")
  cpp = Language("C++", TypeSystem(Weak, Static), false, false, "classes")
  ocaml = Language("OCaml", TypeSystem(Strong, Static), true, true, "classes and modules")
  haskell = Language("Haskell", TypeSystem(Strong, Static), true, true, "typeclasses")
  elixir = Language("Elixir", TypeSystem(Strong, Dynamic), false, true, "protocols")
  rust = Language("Rust", TypeSystem(Strong, Static), true, true, "traits")
  python = Language("Python", TypeSystem(Strong, Dynamic), false, false, "classes")

  print(bon)
  print(cpp)
  print(ocaml)
  print(haskell)
  print(elixir)
  print(rust)
  print(python)

main()
```

Output:

```bash
Bon is a programming language with:
  - Strong, static type system
  - Type Inference
  - Pattern Matching
  - Support for polymorphism through typeclasses

C++ is a programming language with:
  - Weak, static type system
  - Support for polymorphism through classes

OCaml is a programming language with:
  - Strong, static type system
  - Type Inference
  - Pattern Matching
  - Support for polymorphism through classes and modules

Haskell is a programming language with:
  - Strong, static type system
  - Type Inference
  - Pattern Matching
  - Support for polymorphism through typeclasses

Elixir is a programming language with:
  - Strong, dynamic type system
  - Pattern Matching
  - Support for polymorphism through protocols

Rust is a programming language with:
  - Strong, static type system
  - Type Inference
  - Pattern Matching
  - Support for polymorphism through traits

Python is a programming language with:
  - Strong, dynamic type system
  - Support for polymorphism through classes
```
