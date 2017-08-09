#include "positive.h"
#include "foo.h"
#include "foo_inline.h"
#include "foo_macro.h"
#include "foo_type.h"
#include "foo_behind_macro.h"
#include "../test/foo_dot.h"
#include "foo_typedef.h"

#include "aliased_type.h" // required for size of TypeAlias
#include "type_alias.h"

#include "extern_func.h"
#include "extern_func_2.h"
#include "extern_var.h"

#include "unused.h" /* include:allowed */

typedef struct _FooStruct FooStruct; // must be before "foo_struct.h" include

#include "foo_struct.h"
#include "foo_struct_2.h"
#include "foo_struct_3.h"
#include "foo_struct_4_api.h"

#include "../../private/private_api.h"
FRIEND_OF(FooStruct3);
FRIEND_OF(FooStruct4);

__thread struct PositiveDummyType foo_var;

struct Something {
    struct FooType field;
    int x;
    FooTypedef y;
};


struct Something struct_access(const FooStruct *foo);
struct Something struct_access(const FooStruct *foo)
{
    FooStruct2 nothing;
    nothing.field = 17;
    return (struct Something){
        .x = foo_dot(foo->field) + nothing.field,
    };
}

#define HIDE_MACRO FOO_CONSTANT

extern int goo(TypeAlias *x);

int other_foo(int x) {
    TypeAlias alias;
    int y = foo(x);
    struct Something thing = { .x = y + goo(&alias) };
    return foo_dot(behind_macro(foo_inline(x + HIDE_MACRO + thing.x)));
}

int foo_nested(FooStruct3 *foo3);
int foo_nested(FooStruct3 *foo3) {
    return foo3->foo4->field;
}

int extern_var = 3;

int extern_func(int x) {
    return x + 3;
}

int extern_func_usage(void);
int extern_func_usage(void) {
    return extern_func_2(3);
}
