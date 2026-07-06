#include <stdio.h>
#include <stdlib.h>

void emit();

int main() {
    long x = 10;
    printf("%ld\n", x);
    emit();
    return 0;
}

void emit() {
{
    printf("%ld\n", 42);
}
}

