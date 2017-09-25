#include <stdbool.h>

#include "test_enums_conersion_common.h"

struct bar {
    unsigned int val;
};

void g(struct bar *b);
void g(struct bar *b)
{
    e(b->val);
}
