#include "negative.h"

void test_wrong_tag(struct A a);
void test_wrong_tag(struct A a)
{
    int res;
    switch (a.wrong_tag) {
    case E_NOTHING:
        break;
    case E_JUST:
        res = a.just;
        break;
    default:
        break;
    }
}

