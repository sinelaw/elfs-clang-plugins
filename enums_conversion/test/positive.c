#include <stdbool.h>

#include "test_enums_conersion_common.h"

void g(percent x, percent y, enum E e_arg);
void g(percent x, percent y, enum E e_arg)
{
    int num = 3;
    f(num, num + 1);
    p(x, y);
    e(A);
    e(e_arg);
    /* e(true); */
    struct S s;
    s.tag = A;
    e(s.tag);
    e((&s)->tag);
    e(*(&s.tag));
    e((x < y) ? A : B);

    e(A | B);
    e(A | B | A);

    e(A & B);

    // More than 4 arguments are not supported.
    // Save them to typed variable:
    /* e(A | B | A | B); */
    /* e(A | B | A | B | A); */
    const enum E quad = (A | B | A | B);
    e(quad);

    flags(F0);
    flags(F0 | F1);
    flags(F0 | FF);
    flags(F0 & F1);
    flags(F0 & FF);

    if (e_arg == B) {
    } else if (A != e_arg) {
    }

    int z __attribute__((unused)) = A == e_arg ? 0 : 1;

    const bool b = false;
    e(b ? A : B);
    e((b ? A : B));
    e(b ? A : e_arg);
    e((b ? A : e_arg));
    e(b ? e_arg : B);
    e((b ? e_arg : B));
    e(b ? e_arg : e_arg);
    e((b ? e_arg : e_arg));

    const enum E e_val = A;
    e(b ? A : e_val);
    e((b ? A : e_val));
    e(b ? e_val : B);
    e((b ? e_val : B));
    e(b ? e_val : e_val);
    e((b ? e_val : e_val));

    e((!b) ? e_val : e_val);
    e((!b ? (A | B) : (A | B)));


    if (F0 & FF) {
        // should be ok.

    }

    if (F0 & 3) {
    }

    switch (s.tag) {
    case A: break;
    case B: break;
    default: break;
    }

    if (A == B) {
    }
}
