# Basic Clang LLVM IR instrumentation for AFL

A basic instrumentation pass for LLVM IR to add AFL-style coverage.

# Usage

```
clang simple.c -S -emit-llvm -o simple.ll
clang++ Pass.cpp -fPIC -shared -o Pass.so -fno-rtti
opt -enable=new-pm=0 -load "$PWD/Pass.so" -afl-coverage simple.ll -S -o simple_afl.ll
clang simple_afl.ll -o simple_afl
```

# Environment

```
clang version 16.0.6
```

# Sources

The `Pass.cpp` code was adapted from László Szekeres at
<https://groups.google.com/g/afl-users/c/gpa_igE8G50>.