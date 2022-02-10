## Installing Bon

### Run using docker container

The easiest way to check the language out is to run the docker container:
```bash
$ git clone https://github.com/FBMachine/bon.git
$ cd bon
$ bon_docker
```

### Manual build

To build Bon, you'll first need to install these dependencies:
- CMake (at least version 3.4.3)
- LLVM Development toolchain (i.e. install the `llvm-dev` package using `apt` in Ubuntu, or build from source from [here](http://releases.llvm.org)). Currently, the only version tested is 4.0.1, so your mileage may vary if you build against a different version.

To test that everything installed properly:

```bash
$ bon --version
Bon <version_number>
$ bon examples/hello.bon
Hello, world!
```

Bon is in a very early stage of development, so building is likely to only work on Linux and Mac OS. Help with testing and supporting other platforms would be greatly appreciated.

### Next steps

Check out the [Hello World](ch01-02-hello-world.md) tutorial to get started, or head to the [quick tour](ch01-03-quick-tour.md) for a more thorough introduction to Bon's core features.
