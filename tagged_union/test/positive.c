#include <unistd.h>

#define TAGGED_UNION(tag_name) struct {} tagged_union__ ## tag_name

enum E {
    E_NOTHING,
    E_JUST,
};

struct A {
    union {
        TAGGED_UNION(tag);
        int just;
    };
    enum E tag;
    enum E wrong_tag;
};

static int id(int x) {
    return x;
}

static int id2(int x, int y) {
    return x + y;
}

int test(struct A a);
int test(struct A a)
{
    int res = 0;
    switch (a.tag) {
    case E_NOTHING:
        break;
    case E_JUST:
        res = a.just;
        break;
    default:
        break;
    }
    return res;
}

int test_nonmemb_switch(struct A a);
int test_nonmemb_switch(struct A a)
{
    int res = 0;
    switch (a.tag) {
    case E_NOTHING:
        break;
    case E_JUST: {
        enum E x = E_JUST;
        switch (x) {
        case E_JUST:
            res = a.just;
        case E_NOTHING:
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
    return res;
}

/* NOT SUPPORTED! TODO implement
void test_array(struct A *a);
void test_array(struct A *a)
{
    int res = 0;
    switch (a[0].tag) {
    case E_NOTHING:
        break;
    case E_JUST:
        res = a[0].just;
        break;
    default:
        break;
    }
}
*/

/* struct A test_rvalue(void); */
/* struct A test_rvalue(void) */
/* { */
/*     struct A a; */
/*     a.just = 2; */
/*     return a; */
/* } */

int test_ptr_struct(struct A *a);
int test_ptr_struct(struct A *a)
{
    int res = 0;
    switch (a->tag) {
    case E_NOTHING:
        break;
    case E_JUST:
        res = a->just;
        break;
    default:
        break;
    }
    return res;
}

int test_return(struct A a);
int test_return(struct A a)
{
    switch (a.tag) {
    case E_NOTHING:
        break;
    case E_JUST:
        return a.just;
    default:
        break;
    }
    return 0;
}

void test_second_stmt(struct A a);
void test_second_stmt(struct A a)
{
    int res = 0;
    switch (a.tag) {
    case E_NOTHING:
        break;
    case E_JUST:
        res = 1;
        res += a.just;
        break;
    default:
        break;
    }
}

int test_call(struct A a);
int test_call(struct A a)
{
    int res = 0;
    switch (a.tag) {
    case E_NOTHING:
        break;
    case E_JUST:
        res = id(a.just);
        break;
    default:
        break;
    }
    return res;
}

int test_call2(struct A a);
int test_call2(struct A a)
{
    int res = 0;
    switch (a.tag) {
    case E_NOTHING:
        break;
    case E_JUST:
        res = id2(0, a.just);
        break;
    default:
        break;
    }
    return res;
}

int test_ref(struct A a);
int test_ref(struct A a)
{
    int res = 0;
    switch (a.tag) {
    case E_NOTHING:
        break;
    case E_JUST:
        res = (int)(unsigned long)(&a.just);
        break;
    default:
        break;
    }
    return res;
}

int test_call_ref(struct A a);
int test_call_ref(struct A a)
{
    int res = 0;
    switch (a.tag) {
    case E_NOTHING:
        break;
    case E_JUST:
        res = id2(1, (int)(unsigned long)(&a.just));
        break;
    default:
        break;
    }
    return res;
}

int test_decl_var(struct A a);
int test_decl_var(struct A a)
{
    int res = 0;
    switch (a.tag) {
    case E_NOTHING:
        break;
    case E_JUST: {
        int moshe = a.just;
        res = moshe;
        break;
    }
    default:
        break;
    }
    return res;
}

enum E2 {
    E2_JOTHING,
    E2_JUST,
};

struct A2 {
    union {
        TAGGED_UNION(tag);
        int just;
    };
    enum E2 tag;
};

int test_common_prefix(struct A2 a);
int test_common_prefix(struct A2 a)
{
    int res = 0;
    switch (a.tag) {
    case E2_JOTHING:
        break;
    case E2_JUST:
        res = a.just;
        break;
    default:
        break;
    }
    return res;
}

int test_equals(struct A2 a, struct A2 b);
int test_equals(struct A2 a, struct A2 b)
{
    int res = 0;
    if (a.tag == b.tag) {
        switch (a.tag) {
        case E2_JOTHING:
            break;
        case E2_JUST:
            res = a.just + b.just;
            break;
        default:
            break;
        }
    }
    if (a.tag != b.tag) {
        // bla
    } else {
        switch (a.tag) {
        case E2_JOTHING:
            break;
        case E2_JUST:
            res = a.just + b.just;
        break;
        default:
            break;
        }
    }
    return res;
}
