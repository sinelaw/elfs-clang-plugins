#include <stdbool.h>

#include "test_enums_conersion_common.h"

bool g(enum E x);
bool g(enum E x)
{
    return (!x);
}

