#include <stdbool.h>

enum A {
    A_1 = 0,
    A_2,
};

enum B {
    B_1 = 0,
    B_2,
};

int g(enum A a);
int g(enum A a)
{
    switch (a) {
    case A_1: return 2;
    case B_2: return 3;
    default: return 4;
    }
}

