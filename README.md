# Precious

A Gollum-themed programming language that compiles to x86-64 assembly.
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
- x86-64 NASM tutorial: https://cs.lmu.edu/~ray/notes/nasmtutorial/
- Lets make a compiler : https://www.youtube.com/playlist?list=PLUDlas_Zy_qC7c5tCgTMYq2idyyT241qs

## Quick Start

```bash
cmake -B build
cmake --build build
./build/precious your_file.precious
./out
```

The compiler outputs a Linux x86-64 ELF binary named `out`.

## Syntax

### Variables

```
we_haves x = 42;
x = 10;
```

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

### Exit Code

`gives(expr)` sets the process exit code to the value of `expr`. Use `echo $?` to check.

## Examples

```
we_haves a = 2;
we_haves b = 3;
gives(a + b * 4);
```

```bash
./build/precious examples/math.precious
./out
echo $?   # prints 14
```

## Running Tests

```bash
bash run_tests.sh
```

