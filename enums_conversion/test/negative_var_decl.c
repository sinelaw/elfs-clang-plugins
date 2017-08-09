#include <stdbool.h>

#include "test_enums_conersion_common.h"

enum E g(void);
enum E g(void)
{
    const enum E res = false;
    return res;
}

