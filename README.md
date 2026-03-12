# Quail Programming Language

Quail is a strongly typed, lazy functional programming language currently in early development.Quail features a Hindley-Milner type system, powerful pattern matching, and lazy evaluation semantics backed by a Graph Reduction Machine (G-Machine).

The Quail toolchain includes `qc`, an Ahead-of-Time (AOT) compiler written in C++ that leverages LLVM to generate optimized native executables, alongside a lightweight C-based runtime.

## Key Features

* **Lazy Evaluation:** Expressions are only evaluated when their results are needed, powered by a custom G-Machine implementation.
* **Strong Static Typing:** A Hindley-Milner type system ensures type safety at compile time without the need for verbose type annotations.
* **Parametric Polymorphism:** Full support for polymorphic functions and polymorphic data types (e.g., generics).
* **Pattern Matching:** Expressive `match ... with` syntax for destructing Algebraic Data Types (ADTs).
* **AOT Compilation:** Compiles directly to native machine code via LLVM, avoiding interpreter overhead.
* **Memory Management:** Automatic garbage collection handles the allocation and cleanup of the G-Machine graph.

## Architecture

The Quail compiler (`qc`) operates through the following pipeline:

1. **Frontend:** Lexing and Parsing (via Flex and Bison) into an Abstract Syntax Tree (AST).
2. **Semantic Analysis:** Hindley-Milner type inference resolves and validates types.
3. **Lowering:** The AST is transformed into G-Machine instructions for lazy evaluation graph reduction.
4. **Backend:** G-Machine instructions are lowered to LLVM IR.
5. **Code Generation:** LLVM optimizes the IR and compiles it into a native executable, linking it against the Quail C runtime (which handles graph allocation, garbage collection, and I/O).

## Syntax Overview

### Algebraic Data Types (ADTs)

Types are defined using the `type` keyword, allowing for polymorphic type variables and multiple constructors separated by commas:

```quail
type Option a = { 
    None, 
    Some a 
}

type List a = { 
    Nil, 
    Cons a (List a) 
}

```

### Functions and Pattern Matching

Functions are defined using the `fun` keyword. Pattern matching is handled via the `match ... with` construct:

```quail
fun map f xs = {
    match xs with {
        Nil -> { Nil }
        Cons head tail -> { Cons (f head) (map f tail) }
    }
}

fun add x y = {
    x + y
}

```

## Building from Source

The Quail compiler (`qc`) is built using CMake. You will need a C++ compiler that supports C++23 (or later), LLVM development libraries, Flex, and Bison.

### Prerequisites

* CMake (>= 3.30)
* LLVM (>= 21.1.8)
* Flex & Bison
* A C++ Compiler (GCC, Clang, or MSVC)
* A C Compiler (GCC for the runtime)

### Build Instructions

1. **Clone the repository:**
```bash
git clone https://gitlab.com/papakonstantinou/quail-lang.git
cd quail-lang

```


2. **Generate the build files:**
```bash
mkdir build
cd build
cmake ..

```


3. **Compile the project:**
```bash
make

```



This will produce the `qc` compiler binary in your build directory.
