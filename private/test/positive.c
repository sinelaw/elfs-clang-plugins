#include "positive.h"
#include "foo.h"
#include "friend.h"


#include "../private_api.h"

FRIEND_OF(Friend);

int test(PositiveDummyType *x, struct Foo *foo, Friend *friend);
int test(PositiveDummyType *x, struct Foo *foo, Friend *friend)
{
    return x->private_field + FOO_GET_FIELD(*foo) + friend->private_field;
}
