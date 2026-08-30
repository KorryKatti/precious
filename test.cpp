#include <bits/stdc++.h>

void say_hello();
void add(long a, long b);
void greet(std::string name);
long multiply(long a, long b);
void print_double(long x);
long square(long x);
long double_it(long x);
long add_doubles(long a, long b);
std::string printer(std::vector<std::string>& arr, long n);
long the_precious();

int main() {
    return the_precious();
}

void say_hello()
{
    std::cout << (42) << "\n";
}

void add(long a, long b)
{
    std::cout << (a + b) << "\n";
}

void greet(std::string name)
{
    std::cout << (name) << "\n";
}

long multiply(long a, long b)
{
    return a * b;
}

void print_double(long x)
{
    std::cout << (x * 2) << "\n";
}

long square(long x)
{
    return x * x;
}

long double_it(long x)
{
    return x * 2;
}

long add_doubles(long a, long b)
{
    return double_it(a) + double_it(b);
}

std::string printer(std::vector<std::string>& arr, long n)
{
    return arr[0];
}

long the_precious()
{
    long count = 42;
    std::string greeting = "hello";
    long inferred_num = 99;
    std::string inferred_str = "world";
    long a = 10;
    long b = 3;
    std::cout << (a + b) << "\n";
    std::cout << (a - b) << "\n";
    std::cout << (a * b) << "\n";
    std::cout << (a / b) << "\n";
    std::cout << (a + b * 2) << "\n";
    std::cout << ((a + b) * 2) << "\n";
    std::cout << (a == b) << "\n";
    std::cout << (a != b) << "\n";
    std::cout << (a > b) << "\n";
    std::cout << (a < b) << "\n";
    std::cout << (a >= 10) << "\n";
    std::cout << (b <= 5) << "\n";
    long x = 5;
    if (x > 0 && x < 10) {
    std::cout << (1) << "\n";
}
    if (x == 1 || x == 5) {
    std::cout << (2) << "\n";
}
    if (!(0)) {
    std::cout << (3) << "\n";
}
    std::string name = "gollum";
    std::cout << (name) << "\n";
    std::string msg = "the precious";
    std::cout << (msg) << "\n";
    long score = 85;
    if (score >= 90) {
    std::cout << (100) << "\n";
}
 else if (score >= 80) {
    std::cout << (90) << "\n";
}
 else if (score >= 70) {
    std::cout << (80) << "\n";
}
 else {
    std::cout << (0) << "\n";
}
    long i = 0;
    while (i < 5) {
    i = i + 1;
}
    std::cout << (i) << "\n";
    long outer = 10;
{
    long inner = 5;
    std::cout << (outer + inner) << "\n";
}
    say_hello();
    add(10, 20);
    greet("precious");
    long product = multiply(6, 7);
    std::cout << (product) << "\n";
    print_double(5);
    long sq = square(8);
    std::cout << (sq) << "\n";
    long result = square(3) + square(4);
    std::cout << (result) << "\n";
    std::vector<std::string> lol = {"yara", "yara", "phonk"};
    std::string saying = printer(lol, 3);
    std::cout << (saying) << "\n";
    long dd = add_doubles(3, 4);
    std::cout << (dd) << "\n";
    std::string title = "master";
    std::cout << (title) << "\n";
    std::vector<long> numbers = {10, 20, 30};
    std::cout << (numbers[0]) << "\n";
    std::cout << (numbers[1]) << "\n";
    std::cout << (numbers[2]) << "\n";
    numbers[1] = 99;
    std::cout << (numbers[1]) << "\n";
    std::vector<std::string> words = {"hello", "world"};
    std::cout << (words[0]) << "\n";
    std::cout << (words[1]) << "\n";
    long idx = 2;
    std::cout << (numbers[idx]) << "\n";
    return 0;
    long o = 0;
    while (o < 100) {
    if (o == 5) {
    break;
}
    o = o + 1;
}
    std::cout << (o) << "\n";
    long j = 0;
    while (j < 6) {
    j = j + 1;
    if (j % 2 == 0) {
    continue;
}
    std::cout << (j) << "\n";
}
    long n = 0;
    while (n < 20) {
    n = n + 1;
    if (n % 2 == 0) {
    continue;
}
    if (n == 9) {
    break;
}
    std::cout << (n) << "\n";
}
    return 0;
}

