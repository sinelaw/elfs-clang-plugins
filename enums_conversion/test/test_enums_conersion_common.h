enum E {
    A, B
};

typedef int newtype__percent;
typedef newtype__percent percent;

void f(int, int);
void p(percent, percent);
void e(enum E);
void b(bool);

struct S {
    enum E tag;
};

enum Flags {
    F0 = 0,
    F1 = 1,
    FF = 0xFFFFFFFF
};

void flags(enum Flags);
