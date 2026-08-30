#include <bits/stdc++.h>

long the_precious();

int main() {
    return the_precious();
}

long the_precious()
{
    long a = 48;
    long b = 18;
    long temp = 0;
    if (a < b) {
    temp = a;
    a = b;
    b = temp;
}
    std::cout << (a) << "\n";
    std::cout << (b) << "\n";
    long done = 0;
    if (b == 0) {
    done = 1;
}
    if (!(done)) {
    long remainder = 0;
    long counter = 0;
    if (a >= b && !((a < b))) {
    counter = 1;
}
    if (a > b && b > 0) {
    counter = counter + 1;
}
    if (a != b && b != 0) {
    counter = counter + 1;
}
    long check = 0;
    if (counter == 3) {
    check = 100;
}
    if (counter != 3 || a == b) {
    check = check + 1;
}
    std::cout << (counter) << "\n";
    std::cout << (check) << "\n";
    return check;
}
    return 99;
}

