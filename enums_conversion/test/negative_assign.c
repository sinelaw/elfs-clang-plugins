#include <stdbool.h>

#include "test_enums_conersion_common.h"

enum E g(void);
enum E g(void)
{
    enum E res;
    res = false;
    return res;
}
