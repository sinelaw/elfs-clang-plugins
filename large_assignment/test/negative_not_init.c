struct Big {
    int field_int;
    char field_arr[2560];
};

void foo(struct Big *big, const struct Big *big2);
void foo(struct Big *big, const struct Big *big2) {
    *big = *big2;
}
