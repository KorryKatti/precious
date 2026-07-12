#include <stdio.h>
#include <stdlib.h>


int main() {
    const char* s = "a";
    s = "b";
    printf("%s\n", s);
    return 0;
}

