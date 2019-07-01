## Introduction

`dcmalloc` is a very simple malloc(3) implementation meant for gathering
malloc stats. It is deprecated, and there are much better tools to do the
same job such as [Nokia's memory profiler](
https://github.com/nokia/memory-profiler) or
[Valgrind](http://www.valgrind.org/) (if you can afford the application
slowdown).

`dcmalloc` allocates memory by calling the underlying system allocator.

## Building

To compile, just download this repository and run:
```console
make
```

This will produce `dcmalloc.so`, which can be dynamically linked with the
application.

## Usage

If successfully compiled, you can dynamically link dcmalloc with your
application by using LD_PRELOAD (if your application was not statically
linked with another memory allocator).
```console
LD_PRELOAD=dcmalloc.so ./your_application
```

## Copyright

License: MIT

Read file [COPYING](COPYING)

