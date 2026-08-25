# Precious

A Gollum-themed programming language that compiles to C++.
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
./your_file
```

The compiler outputs a Linux ELF binary named after the input file (e.g., `your_file` from `your_file.precious`).

### Why C++ instead of handwritten assembly?

The compiler originally emitted x86-64 NASM assembly directly. That works fine for tiny programs, but as the language grows, handwritten assembly gets painful fast — every new feature means writing dozens of `push`/`pop`/`mov`/`cmp` instructions by hand. Meanwhile g++ with `-O2` does register allocation, instruction combining, and dead code elimination for free. So the compiler now generates C++ source code and lets g++ handle the hard parts. <!--ofc i didnt write this line-->

## Syntax

### Entry Point

Every program defines its entry point as `fn the_precious()` — Precious' equivalent of C's `main`. The compiler generates `int main()`, which calls it and uses `gives` values as the process exit code.

```
fn the_precious() {
    my x = 42;
    gives(x);      // exit code
}
```

Only `fn` definitions may live at top level. Misplaced code produces an error:
- No entry point: `[ERROR] Where is the precious?! Every program needs an entry point: 'fn the_precious() { ... }'`
- Two entry points: `[ERROR] There can be only one precious!`
- Parameters on it: `[ERROR] The precious takes no arguments!`
- Return annotation on it: `[ERROR] The precious needs no return type!`
- Statements outside any fn: `[ERROR] Only 'fn' definitions may live at top level!`

```
fn greet(name) {
    say(name);
}

fn the_precious() {
    greet("precious");
}
```

### Variables

Declare variables with `my`. Assignment uses `=`.

```
my x = 42;
x = 10;
```

### Type Annotations

Optionally annotate variables with a type after `:`. Types are inferred if omitted.

| Precious | C++ type    | Description |
|----------|-------------|-------------|
| `number` | `long`    | 64-bit integers (default) |
| `word`   | `std::string` | Null-terminated strings |
| `question` | `long`  | Booleans (0 or 1) |
| `decimal` | `double` | Floating-point numbers |
| `letter` | `char`    | Single characters |

```
my x: number = 5;           // explicit type
my name: word = "gollum";   // explicit type
my y = 10;                  // inferred as number
my msg = "hello";           // inferred as word
```

The compiler picks the right `printf` format (`%ld` vs `%s`) based on the declared type.

### Arithmetic

```
my result = 2 + 3 * 4 - 1;
my negative = -5;
my combined = -3 + 7;
gives(result);
```

### If / Elif / Else

```
my x = 5;
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
my x = 5;
{
    my y = 10;
    gives(x + y);
}
```

### Comparison Operators

`==`, `!=`, `<`, `>`, `<=`, `>=` — return 1 (true) or 0 (false).

### Boolean Operators

`and`, `or`, `!` — logical connectives for combining conditions.

```
my x = 5;
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
my i = 0;
while (i < 5) {
    i = i + 1;
}
gives(i);
```

### For Loop

C-style for loop with init, condition, and update:

```
for (my i = 0; i < 10; i = i + 1) {
    say(i);
}
```

### For-Each Loop

Iterate over arrays without manual indexing:

```
my arr: number[3] = [10, 20, 30];
for (each item in arr) {
    say(item);
}
```

### Break and Continue

Use `break` to exit a while/for loop early, and `continue` to skip to the next iteration.

```
my i = 0;
while (i < 10) {
    if (i == 5) {
        break;        // exit the loop
    }
    i = i + 1;
}

my sum = 0;
my j = 0;
while (j < 10) {
    j = j + 1;
    if (j % 2 == 0) {
        continue;     // skip even numbers
    }
    sum = sum + j;
}
```

### Switch / Case

Match a value against a list of integer constants. There is no fallthrough — each
`case` runs its own body, then the switch exits. `default` is optional.

```
my x = 2;
switch (x) {
    case 1: say(10);
    case 2: say(20);
    case 3: say(30);
    default: say(99);
}
```

Case values must be integer literals. If no case matches and there is no `default`,
nothing runs. A `default` may appear at most once.

### Exit Code

`gives(expr)` sets the process exit code to the value of `expr`. Use `echo $?` to check.

### Print

`say(expr)` prints the value of `expr` to stdout. Works with integers, string literals, and string variables.

```
my x = 42;
say(x);            // prints 42
say(x + 8);        // prints 50
say("hello");      // prints hello
say("precious");   // prints precious

my msg: word = "gollum";
say(msg);          // prints gollum
```

### String Concatenation

Join strings with `+`:

```
my first = "hello";
my second = " world";
my combined = first + second;
say(combined);     // prints hello world
```

String variables can be reassigned and concatenated freely:

```
my a = "foo";
my b = "bar";
my c = a + b;
a = c + "baz";
say(a);            // prints foobarbaz
```

### String Indexing

Access individual characters with `[]`:

```
my msg = "hello";
say(msg[0]);       // prints h
say(msg[4]);       // prints o
```

### Functions

Define reusable code blocks with `fn`, call them by name.

```
fn greet() {
    say(42);
}

greet();        // prints 42
```

Functions support parameters, with optional type annotations:

```
fn add(a, b) {
    say(a + b);
}
add(2, 3);      // prints 5

fn greet(name: word) {
    say(name);
}
greet("precious");  // prints precious
```

Functions can return values using `gives`:

```
fn add(a, b) {
    gives(a + b);
}

my result = add(2, 3);
say(result);    // prints 5
```

You can also explicitly annotate a function return type with `->`:

```
fn greet() -> word {
    gives("hi");
}

say(greet());   // prints hi
```

Supported return types are the same as variable annotations: `number`, `word`, `question`, `decimal`, and `letter`.

The compiler automatically detects whether a function uses `gives` and emits the correct return type (`long` for returning functions, `void` otherwise) when no explicit annotation is provided. Functions can call each other regardless of declaration order — the compiler emits forward declarations before `main()` and definitions after it.

### Arrays

Indexed collections of values. Arrays require explicit type annotations.

```
my numbers: number[3] = [10, 20, 30];
say(numbers[0]);    // prints 10
say(numbers[2]);    // prints 30

numbers[1] = 99;
say(numbers[1]);    // prints 99
```

String arrays work too:

```
my words: word[2] = ["hello", "world"];
say(words[0]);      // prints hello
```

Use variables as indices:

```
my i: number = 1;
say(numbers[i]);    // prints value at index 1
```

### Array Push / Pop

Add and remove elements dynamically with `push` and `pop`:

```
my arr: number[] = [1, 2, 3];
push arr, 4;
say(arr[3]);        // prints 4
pop arr;
say(arr[2]);        // prints 3
```

Works with string arrays too:

```
my words: word[] = ["hello", "world"];
push words, "foo";
say(words[2]);      // prints foo
pop words;
say(words[1]);      // prints world
```

### Array Parameters to Functions

Pass arrays as function arguments using `type[]` syntax:

```
fn sum(arr: number[], n) {
    my total = 0;
    my i = 0;
    while (i < n) {
        total = total + arr[i];
        i = i + 1;
    }
    gives(total);
}

my nums: number[3] = [10, 20, 30];
say(sum(nums, 3));    // prints 60
```

The compiler passes arrays as `std::vector` references to functions.

## Examples

```
// math.precious
fn the_precious() {
    my a = 2;
    my b = 3;
    gives(a + b * 4);
}
```

```bash
./build/precious examples/math.precious
./math
echo $?   # prints 14
```

```
// greet.precious
fn greet(name) {
    say(name);
}

fn the_precious() {
    my msg: word = "precious";
    greet(msg);
}
```

```bash
./build/precious examples/greet.precious
./greet   # prints: precious
```

## Running Tests

```bash
bash run_tests.sh
```

