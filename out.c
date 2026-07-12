#include <stdio.h>
#include <stdlib.h>

void say_hello();
void add(long a, long b);
void greet(const char* name);
long multiply(long a, long b);
void print_double(long x);
long square(long x);
long double_it(long x);
long add_doubles(long a, long b);

int main() {
    long count = 42;
    const char* greeting = "hello";
    long inferred_num = 99;
    const char* inferred_str = "world";
    long a = 10;
    long b = 3;
    printf("%ld\n", a + b);
    printf("%ld\n", a - b);
    printf("%ld\n", a * b);
    printf("%ld\n", a / b);
    printf("%ld\n", a + b * 2);
    printf("%ld\n", (a + b) * 2);
    printf("%ld\n", a == b);
    printf("%ld\n", a != b);
    printf("%ld\n", a > b);
    printf("%ld\n", a < b);
    printf("%ld\n", a >= 10);
    printf("%ld\n", b <= 5);
    long x = 5;
    if (x > 0 && x < 10)
{
    printf("%ld\n", 1);
}
    if (x == 1 || x == 5)
{
    printf("%ld\n", 2);
}
    if (!(0))
{
    printf("%ld\n", 3);
}
    const char* name = "gollum";
    printf("%s\n", name);
    const char* msg = "the precious";
    printf("%s\n", msg);
    long score = 85;
    if (score >= 90)
{
    printf("%ld\n", 100);
}
 else if (score >= 80)
{
    printf("%ld\n", 90);
}
 else if (score >= 70)
{
    printf("%ld\n", 80);
}
 else
{
    printf("%ld\n", 0);
}
    long i = 0;
    while (i < 5)
{
    i = i + 1;
}
    printf("%ld\n", i);
    long outer = 10;
{
    long inner = 5;
    printf("%ld\n", outer + inner);
}
    say_hello();
    add(10, 20);
    greet("precious");
    long product = multiply(6, 7);
    printf("%ld\n", product);
    print_double(5);
    long sq = square(8);
    printf("%ld\n", sq);
    long result = square(3) + square(4);
    printf("%ld\n", result);
    long dd = add_doubles(3, 4);
    printf("%ld\n", dd);
    const char* title = "master";
    printf("%s\n", title);
    return 0;
    return 0;
}

void say_hello() {
{
    printf("%ld\n", 42);
}
}

void add(long a, long b) {
{
    printf("%ld\n", a + b);
}
}

void greet(const char* name) {
{
    printf("%s\n", name);
}
}

long multiply(long a, long b) {
{
    return a * b;
}
}

void print_double(long x) {
{
    printf("%ld\n", x * 2);
}
}

long square(long x) {
{
    return x * x;
}
}

long double_it(long x) {
{
    return x * 2;
}
}

long add_doubles(long a, long b) {
{
    return double_it(a) + double_it(b);
}
}

