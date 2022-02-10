## Hello World!

Following tradition, we're starting our journey with the simplest possible program.

Filename: hello.bon

```python
print("Hello, world!")
```

While Bon is a compiled language, like scripting languages it supports top-level expressions, so you can write small utilities with minimal friction.

However, it's good practice to have the top-level only run a main function. Note that this is only by convention, and the choice of function name is arbitrary.

Filename: hello_main.bon

```python
def main():
  print("Hello, world!")

main()
```

To run our simple program:

```bash
$ bon hello.bon
Hello, world!
```

[Next](ch01-03-quick-tour.md), we'll take a quick tour of the language in order to introduce the core features.
