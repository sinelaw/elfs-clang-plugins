static int foo(void) {
    return 3;
}
int extern_foo(void);
int extern_foo(void) {
    return foo();
}
