#include "foo.h"

int not_in_foo_h(int);
int not_in_foo_h(int x) {
    return x;
}
