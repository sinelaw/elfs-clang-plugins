#include "negative.h"

void test_wrong_case(struct A a);
void test_wrong_case(struct A a)
{
    int res;
    switch (a.tag) {
    case E_NOTHING:
        res = a.just;
        break;
    case E_JUST:
        break;
    default:
        break;
    }
}

