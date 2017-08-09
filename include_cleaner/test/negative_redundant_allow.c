#include "foo.h" /* include:allowed */

int bar(int);
int bar(int x) {
    return foo(x);
}
