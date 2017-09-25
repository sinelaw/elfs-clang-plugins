#include <stdbool.h>

#include "test_enums_conersion_common.h"

void g(bool b);
void g(bool b)
{
    e(b ? A : 0);
}
