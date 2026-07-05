#include <stdio.h>
#include <stdlib.h>

int main() {
    long a = 48;
    long b = 18;
    long temp = 0;
    if (a < b)
{
    temp = a;
    a = b;
    b = temp;
}
    printf("%ld\n", a);
    printf("%ld\n", b);
    long done = 0;
    if (b == 0)
{
    done = 1;
}
    if (!(done))
{
    long remainder = 0;
    long counter = 0;
    if (a >= b && !((a < b)))
{
    counter = 1;
}
    if (a > b && b > 0)
{
    counter = counter + 1;
}
    if (a != b && b != 0)
{
    counter = counter + 1;
}
    long check = 0;
    if (counter == 3)
{
    check = 100;
}
    if (counter != 3 || a == b)
{
    check = check + 1;
}
    printf("%ld\n", counter);
    printf("%ld\n", check);
    return check;
}
    return 99;
    return 0;
}
