#include <stdio.h>
#include <stdlib.h>

void greet(const char* name);

int main() {
    const char* title = "gollum";
    greet(title);
}

void greet(const char* name)
{
    printf("%s\n", name);
}

