#include <stdio.h>
#include <string.h>

__attribute__((warn_unused_result))
static int foo(void) {
    return 3;
}

int foo_non_static(void); // test config uses --static-only, so this is ok to not have warn_unused_result

void void_func(void);

void use_strcmp(const char *a, const char *b);
void use_strcmp(const char *a, const char *b)
{
    printf("%d %d", strcmp(a, b), foo());
}
