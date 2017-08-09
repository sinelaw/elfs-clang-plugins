#include "positive.h"

int foo(PositiveDummyType *x);
int foo(PositiveDummyType *x)
{
    return x->private_field;
}
