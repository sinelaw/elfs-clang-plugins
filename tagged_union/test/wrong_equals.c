#include "negative.h"

void test_equals(struct A a, struct A b);
void test_equals(struct A a, struct A b)
{
    int res;
    /* if (a.tag == b.tag) { */
    /*     res = 0; */
    /* } */
    switch (a.tag) {
    case E_NOTHING:
        break;
    case E_JUST:
        if (a.tag == b.tag) {
            res = 0;
        } else {
            res = a.just + b.just;
        }
        break;
    default:
        break;
    }
}
