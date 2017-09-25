#include <stdbool.h>

#include "test_enums_conersion_common.h"

bool g(void);
bool g(void)
{
    const bool res = A;
    return res;
}
