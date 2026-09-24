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
* **Optional Annotations:** A parameter or a return type may be written out, one at a time; what is left off is inferred as before.
* **Conditionals:** An `if ... else` expression that branches on a `Bool`.
* **Type Classes:** `class` and `instance` declarations, with superclasses, default methods and inferred contexts, compiled by dictionary passing.
* **Comparisons:** The operators `==`, `!=`, `>`, `<`, `>=` and `<=` are methods of `Eq` and `Ord`, so they weigh any two values of a type that has an instance.
* **Overloaded Arithmetic:** `+`, `-`, `*` and `/` are methods of `Num`, and a written number stands for whatever type it is used at.
* **Built-in Lists:** A primitive `List` type with bracket syntax (`[1, 2, 3]`) for literals.
* **Pipelines:** A `|>` operator that reads a chain of calls front to back.
* **Composition:** A `.` operator that builds one function out of two, the way Haskell's does.
* **AOT Compilation:** Compiles directly to native machine code via LLVM, avoiding interpreter overhead.
* **Memory Management:** Automatic garbage collection handles the allocation and cleanup of the G-Machine graph.

## Architecture

The Quail compiler (`qc`) operates through the following pipeline:

1. **Frontend:** Lexing and Parsing (via Flex and Bison) into an Abstract Syntax Tree (AST).
2. **Semantic Analysis:** Hindley-Milner type inference resolves and validates types, extended with qualified types so that a definition may hold under a class constraint.
3. **Elaboration:** Constraints are solved and each use of an overloaded name is applied to the dictionary that answers it.
4. **Lowering:** The AST is transformed into G-Machine instructions for lazy evaluation graph reduction. A class becomes a data type of one constructor with a selector per field; an instance becomes the function that builds it.
5. **Backend:** G-Machine instructions are lowered to LLVM IR.
6. **Code Generation:** LLVM optimizes the IR and compiles it into a native executable, linking it against the Quail C runtime (which handles graph allocation, garbage collection, and I/O).

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

### Type Annotations

A definition may say what its parameters and its result are. A parameter is
annotated by putting it in parentheses with its type, and the result by
writing the type after the parameters:

```quail
fun add (x: Int) (y: Int) : Int = { x + y }

fun main = { add 15 27 }
```

Nothing has to be annotated. Each parameter and the return type are separate
choices, so a signature can pin down only the part worth spelling out and
leave the rest to inference:

```quail
fun scale n (factor: Int) = { n * factor }

fun answer : Int = { 42 }
```

Any type may be written: a built-in one, a declared one applied to its
arguments, or a function type. Parentheses group as they do in an expression:

```quail
fun sum (xs: List Int) : Int = { foldr (\a b -> { a + b }) 0 xs }

fun unwrap (m: Maybe Int) (fallback: Int) : Int = {
    match m with {
        Nothing -> { fallback }
        Just v -> { v }
    }
}

fun apply (f: Int -> Int) (x: Int) : Int = { f (f x) }
```

A lowercase name in a signature is a type variable, as it is in a `type`
declaration, and stands for whatever fits. It belongs to the definition that
wrote it, so two definitions do not share a type by both writing `a`:

```quail
type Pair a b = { MkPair a b }

fun first (p: Pair a b) : a = { match p with { MkPair l r -> { l } } }
```

An annotation constrains inference rather than replacing it. A parameter
holds its declared type everywhere in the body, so the use that disagrees is
the one reported, and a body that does not have the declared return type is
reported against the type the definition committed to:

```quail
fun add (x: Int) (y: Int) : Int = { True }
```

```
an error occured while checking the types of the program: the body of add does not have its declared return type
```

`let` bindings are ordinary definitions, so they may be annotated the same
way. Lambdas may not; they are written `\x -> { ... }` and are always
inferred.

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

### Composition

`.` joins two functions into one. `f . g` is the function that hands its
argument to `g` and `g`'s answer to `f`, so `(f . g) x` means `f (g x)`. The
two must fit together: what the right side hands back is what the left side
takes.

```quail
fun double x = { x * 2 }
fun inc x = { x + 1 }

fun main = { (double . inc) 20 }
```

It binds tighter than the arithmetic operators and looser than application,
and associates to the right, so `f . g . h` is `f . (g . h)` and `f . g 1` is
`f . (g 1)`. Either side may be any expression that evaluates to a function,
including a partial application or a constructor:

```quail
fun add x y = { x + y }
fun sum l = { foldr (\x acc -> { x + acc }) 0 l }

fun main = { [1, 2, 3] |> sum . map (add 1) }
```

A composed function is an ordinary value: it may be handed around, applied
later, or built out of functions whose types are not yet fixed.

```quail
fun twice f = { f . f }
```

### Type Classes

A `class` says what a type has to provide, and an `instance` provides it for
one type. A class abstracts over exactly one type variable:

```quail
class Show a = {
    fun show (x: a) : Int
}

instance Show Int = {
    fun show x = { x }
}
```

Every method signature has to be written out in full, and has to mention the
variable the class is about. A method may carry a body as well, which is the
default an instance gets when it does not write its own:

```quail
class Eq a = {
    fun eq (x: a) (y: a) : Bool
    fun neq (x: a) (y: a) : Bool = { not (eq x y) }
}
```

A class may require that an instance of it is already an instance of another,
which is written the way a constraint is. A method may then use the
superclass's methods:

```quail
class Eq a => Ord a = {
    fun lt (x: a) (y: a) : Bool
    fun le (x: a) (y: a) : Bool = { if lt x y { True } else { eq x y } }
}
```

An instance may itself hold only under a constraint. Its head is a type
constructor applied to distinct type variables, and its context says what
those variables must be:

```quail
instance Eq a => Eq (List a) = {
    fun eq xs ys = {
        match xs with {
            Nil -> { match ys with { Nil -> { True } Cons y r -> { False } } }
            Cons x r -> {
                match ys with {
                    Nil -> { False }
                    Cons y s -> { if eq x y { eq r s } else { False } }
                }
            }
        }
    }
}
```

### Constraints on definitions

A definition that uses a method holds under whatever that method needs, and
inference works out what:

```quail
fun member x xs = {
    match xs with {
        Nil -> { False }
        Cons y ys -> { if eq x y { True } else { member x ys } }
    }
}
```

```
member : Eq a => a -> List a -> Bool
```

A context may also be written, between `fun` and the name, so that it reads
in the same order as in a `class` or an `instance`. One constraint may be
written bare and several in parentheses:

```quail
fun Eq a => same (x: a) (y: a) : Bool = { eq x y }

fun (Eq a, Ord b) => rank (x: a) (y: b) : Int = { 1 }
```

A written context is checked against what the body turned out to need, and a
superclass counts as provided: a body needing `Eq a` is satisfied by a
declared `Ord a`. A body needing more than was declared is reported:

```quail
fun Eq a => bad (x: a) (y: a) : Bool = { lt x y }
```

```
an error occured while checking the types of the program: the body needs Ord a, which the declared context Eq a does not provide
```

Only a definition that stands on its own may write a context. Two definitions
that are mutually recursive are worked out together, so a context on one of
them would be a claim about both; leave it off and it is inferred.

### What the prelude provides

`Eq`, `Ord` and `Num`, with instances for `Int`, `Bool`, `List a` and
`Maybe a` where they make sense. The operators are surface syntax for their
methods:

| Operator | Method | Class |
| --- | --- | --- |
| `==` `!=` | `eq` `neq` | `Eq` |
| `<` `<=` `>` `>=` | `lt` `le` `gt` `ge` | `Ord` |
| `+` `-` `*` `/` | `add` `sub` `mul` `div` | `Num` |

An operator goes on meaning its method even where something else has taken
the method's name, so `fun add x y = { x * y }` defines `add` and does not
redefine `+`.

A written number is `Num a => a`, not `Int`: it stands for whatever type it
is used at. Where nothing says which, the defaulting rules settle it, and
`Int` is what they settle on. So `fun double x = { x + x }` is
`Num a => a -> a`, while `fun two = { 1 + 1 }` is `Int`, because a definition
that takes no arguments is a value and a value is computed once.

Writing a signature on a numeric function is worth doing when it is only ever
used at one type: `fun loop (n: Int) (acc: Int) : Int` is compiled against
the dictionary for `Int`, which is known, and the calls become direct ones.
Left to inference the same loop is about any number at all and pays for a
dictionary it is handed.

### Limitations

* A class abstracts over exactly one type variable. There are no
  multi-parameter classes and no functional dependencies.
* An instance head is a type constructor applied to distinct type variables,
  so `instance Eq (List a)` is allowed and `instance Eq (List Int)` is not.
* No two instances of a class may overlap, and there is no way to say which
  should win.
* An instance context is subject to Paterson's conditions, so that working
  out which instances apply always finishes.
* Two classes may not declare a method of the same name.
* A method the class supplies a default for is not specialised as far as one
  the instance writes: it is still reached through the dictionary.
* Deriving is not implemented; every instance is written out.

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
  -o <path>       Specify the output file path
  --dump-ast      Print the structure of the parsed program
  --dump-source   Print the parsed program back out as source
  --check-classes Check the classes and instances and print them
  --dump-types    Print the type inferred for every global
  --dump-core     Print the program with its dictionaries made explicit
  --help          Display this information
```

## Diagnostics

Setting `QUAIL_STATS` in the environment of a compiled program makes it report
what it allocated and how often it collected, which is what the benchmarks in
`test/e2e/bench/` are checked against:

```bash
QUAIL_STATS=1 ./a.out
```

## Acknowledgements

Quail’s Compiler builds upon the techniques described in [Implementing Functional Languages: A Tutorial](https://www.microsoft.com/en-us/research/wp-content/uploads/1992/01/student.pdf) and the blog series [Compiling a Functional Language Using C++](https://danilafe.com/blog/00_compiler_intro/). These resources were invaluable references throughout the development of this project.
