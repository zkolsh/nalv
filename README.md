# Nalv

Nalv is a tool for generating CApiFFI bindings for Haskell out of C source files.  It is in a very early state, and a lot of necessary features are yet to be implemented.  Right now, it handles functions, typedefs, enums, and structs.  However, unions and `#define`s are planned for the near future.

To use this program, pass the name of one file as an argument:
```
$ nalv file.h
```

This will generate the bindings in `build/File.hs`.

## Building

Install Clang with libclang (only tested on clang version 21.1.8; but most likely works on older versions, requires C++20.)

Then, simply compile the project:
```
$ make all
```
