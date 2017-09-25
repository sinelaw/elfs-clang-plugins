#include "test_enums_conersion_common.h"

enum NOT_E { NOT_A } ;

void func(enum E res, unsigned int x);
void func(enum E res, unsigned int x)
{
    if (x == res) { /* comparing enum to arbitrary integer */
    }
}
