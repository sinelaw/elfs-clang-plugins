struct Big {
    int field_int;
    char field_arr[2560];
};

void foo(struct Big big);
void call_foo(void);
void call_foo(void) {
    struct Big big;
    big.field_int = 3;
    foo(big);
}
