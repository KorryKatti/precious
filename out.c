#include <stdio.h>
#include <stdlib.h>

long max(long a, long b);

int main() {
    long x = max(10, 20);
    long y = max(30, 15);
    printf("%ld\n", x);
    printf("%ld\n", y);
    return 0;
}

long max(long a, long b) {
{
    if (a > b)
{
    return a;
}
 else
{
    return b;
}
}
}

