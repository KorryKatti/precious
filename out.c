#include <stdio.h>
#include <stdlib.h>


int main() {
    long digits[3] = {1, 2, 3};
    long n = 3;
    long i = n - 1;
    while (i >= 0) {
    if (digits[i] < 9) {
    digits[i] = digits[i] + 1;
    long idx = 0;
    while (idx < n) {
    printf("%ld\n", digits[idx]);
    idx = idx + 1;
}
    return 1;
}
    digits[i] = 0;
    i = i - 1;
}
    printf("%ld\n", 1);
    long idx = 0;
    while (idx < n) {
    printf("%ld\n", digits[idx]);
    idx = idx + 1;
}
    return 1;
}

