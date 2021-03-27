# the CRADLE C++ preprocessor

## Installing OCaml

This is written in OCaml. If you don't already have it on your system, you
should be able to get it with a package manager.

### Ubuntu Linux

```shell
sudo apt-get install -y ocaml-nox
```

### Windows (via Chocolatey)

```shell
choco install ocpwin
```

## Building

At the moment, in order to avoid complicating the dependencies for the C++
projects that rely on it, this project uses CMake as its build system.

```shell
mkdir build
cd build
cmake ..
cmake --build .
```

Note that since it's not actually invoking any C++ tools, the generator that
you use really doesn't matter.
