# Quail Programming Language

> [!WARNING]  
> The language is still in early development

Quail is a strongly typed, lazy functional programming language currently in early development.Quail features a Hindley-Milner type system, powerful pattern matching, and lazy evaluation semantics backed by a Graph Reduction Machine (G-Machine).

The Quail toolchain includes `qc`, an Ahead-of-Time (AOT) compiler written in C++ that leverages LLVM to generate optimized native executables, alongside a lightweight C-based runtime.

## Key Features

* **Lazy Evaluation:** Expressions are only evaluated when their results are needed, powered by a custom G-Machine implementation.
* **Strong Static Typing:** A Hindley-Milner type system ensures type safety at compile time without the need for verbose type annotations.
* **Parametric Polymorphism:** Full support for polymorphic functions and polymorphic data types (e.g., generics).
* **Pattern Matching:** Expressive `match ... with` syntax for destructing Algebraic Data Types (ADTs).
* **Conditionals:** An `if ... else` expression that branches on a `Bool`.
* **Comparisons:** The operators `==`, `!=`, `>`, `<`, `>=` and `<=` weigh two `Int`s against each other and answer with a `Bool`.
* **Built-in Lists:** A primitive `List` type with bracket syntax (`[1, 2, 3]`) for literals.
* **Pipelines:** A `|>` operator that reads a chain of calls front to back.
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

type Tree a = { 
    Leaf, 
    Node a (Tree a) (Tree a) 
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

### Comparisons

Two `Int`s are weighed against each other with `==`, `!=`, `>`, `<`, `>=` or
`<=`. The answer is an ordinary `Bool`, so it may be branched on, matched on,
or passed around like any other value:

```quail
fun isPositive n = { n > 0 }

fun sign n = {
    match n == 0 with {
        True -> { 0 }
        False -> { if n > 0 { 1 } else { 0 - 1 } }
    }
}
```

A comparison binds looser than arithmetic, so both sides are worked out
before they are weighed: `1 + 2 == 3` compares `3` with `3`. Only `|>` binds
looser still.

Comparisons do not chain. `a < b < c` would ask a `Bool` to stand where an
`Int` belongs, so it is rejected outright rather than left to the type
checker. Two of them are joined by branching on the first:

```quail
fun between low x high = {
    if low < x {
        x < high
    } else {
        False
    }
}
```

### Conditionals

`if` branches on a `Bool` and hands back the value of the branch it took, so
both branches must have the same type. The `else` is not optional: an if is an
expression, and there is nothing for it to stand for when the condition is
false. Only the branch that is taken is ever evaluated:

```quail
fun abs x = {
    if x < 0 {
        0 - x
    } else {
        x
    }
}
```

Being an expression, it may stand wherever one may, including inside another
one. An `else` belongs to the nearest `if`:

```quail
fun classify b c = {
    [if b { 1 } else { if c { 2 } else { 3 } }]
}
```

### Lists

`List` is built into the compiler, with the constructors `Nil` and `Cons`.
A list is written between brackets, and its elements must all have the same
type:

```quail
fun numbers = { [1, 2, 3, 4] }

fun flags = { [True, True, False] }

fun empty = { [] }
```

The brackets are shorthand: `[1, 2, 3]` builds exactly the same graph as
`Cons 1 (Cons 2 (Cons 3 Nil))`, and both spellings may be mixed freely.
Lists are taken apart by matching on the two constructors:

```quail
fun sum l = {
    match l with {
        Nil -> { 0 }
        Cons x xs -> { x + sum xs }
    }
}
```

### Lambdas

Anonymous functions are written with a backslash, a parameter list, and a
braced body. They may capture variables from the enclosing scope:

```quail
fun sum l = { foldr (\x acc -> { x + acc }) 0 l }

fun addToAll n l = { map (\x -> { n + x }) l }
```

### Pipelines

`|>` passes the value on its left to the function on its right, so `x |> f`
means `f x`. It binds looser than everything else and associates to the left,
which lets a chain of calls be read in the order it happens:

```quail
fun sum l = { foldr (\x acc -> { x + acc }) 0 l }

fun main = { [1, 2, 3, 4] |> map (\x -> { x * 2 }) |> sum }
```

The right side may be any expression that evaluates to a function, including
a partial application, so the piped value fills the last argument:

```quail
fun add x y = { x + y }

fun main = { [1, 2, 3, 4] |> foldr add 0 }
```

### Let/In

`let` introduces local definitions visible only inside its `in` block. Each
binding is an ordinary `fun` definition, so it may take parameters, and it may
be recursive or mutually recursive with its siblings:

```quail
fun main = {
    let {
        fun square x = { x * x }
        fun total l = {
            match l with {
                Nil -> { 0 }
                Cons x xs -> { square x + total xs }
            }
        }
    } in {
        total [1, 2, 3]
    }
}
```

Both lambdas and `let` bindings are lambda-lifted: the compiler turns each one
into a global function that takes its captured variables as extra parameters,
and leaves a partial application behind at the original site. `let` bindings
are also generalized, so a binding used at two different types type-checks.

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

## Usage 

```bash
Usage: qc [source_file] [options]
Options:
  -o <path>      Specify the output file path
  --help         Display this information
```

## Acknowledgements

Quail’s Compiler builds upon the techniques described in [Implementing Functional Languages: A Tutorial](https://www.microsoft.com/en-us/research/wp-content/uploads/1992/01/student.pdf) and the blog series [Compiling a Functional Language Using C++](https://danilafe.com/blog/00_compiler_intro/). These resources were invaluable references throughout the development of this project.
