#include <bits/stdc++.h>

long fib(long n);
long the_precious();

int main() {
    return the_precious();
}

long fib(long n)
{
    if (n < 2) {
    return n;
}
    return fib(n - 1) + fib(n - 2);
}

long the_precious()
{
    long i = 0;
    while (i < 10) {
    std::cout << (fib(i)) << "\n";
    i = i + 1;
}
    return 0;
}

