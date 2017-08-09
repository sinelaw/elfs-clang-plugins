struct Big {
    int field_int;
    char field_arr[256];
};

void foo(struct Big *big);
void foo(struct Big *big) {
    big->field_int = 3;
}
