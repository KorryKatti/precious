# Precious

A Gollum-themed programming language that compiles to C.
<!--
i might change the theme if it gets annoying ngl idk it was just funny at that time
 -->
<img src="https://i.pinimg.com/736x/8a/f8/82/8af8823d4a46d75658b84cbaac4c92ff.jpg"
     style="width: 35%; height: auto;">

## Notes
- this will take me a long time
- i have no idea why i am doing this but its fun
- this <a href="https://www.youtube.com/playlist?list=PLUDlas_Zy_qC7c5tCgTMYq2idyyT241qs" target="_blank"> playlist </a> was very helpful
- i am still very new to all this so i might make naive mistakes


**Resources:**
- Pratt parsing: https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html
- Crafting Interpreters (free book, covers parsing + codegen): https://craftinginterpreters.com/
- Lets make a compiler : https://www.youtube.com/playlist?list=PLUDlas_Zy_qC7c5tCgTMYq2idyyT241qs

## Quick Start

```bash
cmake -B build
cmake --build build
./build/precious your_file.precious
./out
```

The compiler outputs a Linux ELF binary named `out` (via gcc).

### Why C instead of handwritten assembly?

The compiler originally emitted x86-64 NASM assembly directly. That works fine for tiny programs, but as the language grows, handwritten assembly gets painful fast — every new feature means writing dozens of `push`/`pop`/`mov`/`cmp` instructions by hand. Meanwhile gcc with `-O2` does register allocation, instruction combining, and dead code elimination for free. So the compiler now generates C source code and lets gcc handle the hard parts. <!--ofc i didnt write this line-->

## Syntax

### Variables

Declare variables with `we_haves`. Assignment uses `=`.

```
we_haves x = 42;
x = 10;
```

### Type Annotations

Optionally annotate variables with a type after `:`. Types are inferred if omitted.

| Precious | C type    | Description |
|----------|-----------|-------------|
| `number` | `long`    | 64-bit integers (default) |
| `word`   | `const char*` | Null-terminated strings |
| `question` | `long`  | Booleans (0 or 1) |
| `decimal` | `double` | Floating-point numbers |
| `letter` | `char`    | Single characters |

```
we_haves x: number = 5;           // explicit type
we_haves name: word = "gollum";   // explicit type
we_haves y = 10;                  // inferred as number
we_haves msg = "hello";           // inferred as word
```

The compiler picks the right `printf` format (`%ld` vs `%s`) based on the declared type.

### Arithmetic

```
we_haves result = 2 + 3 * 4 - 1;
gives(result);
```

### If / Elif / Else

```
we_haves x = 5;
if (x == 5) {
    gives(10);
} elif (x > 3) {
    gives(20);
} else {
    gives(30);
}
```

### Scopes

```
we_haves x = 5;
{
    we_haves y = 10;
    gives(x + y);
}
```

### Comparison Operators

`==`, `!=`, `<`, `>`, `<=`, `>=` — return 1 (true) or 0 (false).

### Boolean Operators

`and`, `or`, `!` — logical connectives for combining conditions.

```
we_haves x = 5;
if (x > 0 and x < 10) {
    gives(1);
}
if (!0) {
    gives(2);
}
if (x == 1 or x == 5) {
    gives(3);
}
```

Precedence: `!` (tightest) > `and` > `or` (loosest).

### While Loop

```
we_haves i = 0;
while (i < 5) {
    i = i + 1;
}
gives(i);
```

### Exit Code

`gives(expr)` sets the process exit code to the value of `expr`. Use `echo $?` to check.

### Print

`say(expr)` prints the value of `expr` to stdout. Works with integers, string literals, and string variables.

```
we_haves x = 42;
say(x);            // prints 42
say(x + 8);        // prints 50
say("hello");      // prints hello
say("precious");   // prints precious

we_haves msg: word = "gollum";
say(msg);          // prints gollum
```

### Functions

Define reusable code blocks with `fn`, call them by name.

```
fn greet() {
    say(42);
}

greet();        // prints 42
```

Functions support parameters:

```
fn add(a, b) {
    say(a + b);
}

add(2, 3);      // prints 5
```

Functions can return values using `gives`:

```
fn add(a, b) {
    gives(a + b);
}

we_haves result = add(2, 3);
say(result);    // prints 5
```

The compiler automatically detects whether a function uses `gives` and emits the correct return type (`long` for returning functions, `void` otherwise). Functions can call each other regardless of declaration order — the compiler emits forward declarations before `main()` and definitions after it.

## Examples

```
// math.precious
we_haves a = 2;
we_haves b = 3;
gives(a + b * 4);
```

```bash
./build/precious examples/math.precious
./out
echo $?   # prints 14
```

```
// greet.precious
fn greet(name) {
    say(name);
}

we_haves msg: word = "precious";
greet(msg);
```

```bash
./build/precious examples/greet.precious
./out   # prints: precious
```

## Running Tests

```bash
bash run_tests.sh
```

