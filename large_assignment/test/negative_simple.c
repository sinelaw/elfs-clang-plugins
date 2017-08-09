struct Big {
    int field_int;
    char field_arr[2560];
};

void foo(struct Big *big);
void foo(struct Big *big) {
    *big = (struct Big){
        .field_int = 3,
    };
}
