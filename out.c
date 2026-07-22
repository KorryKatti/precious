#include <stdio.h>
#include <stdlib.h>

const char* greet();

int main() {
    printf("%s\n", greet());
}

const char* greet()
{
    return "hi";
}

