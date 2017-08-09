#ifndef __ELFS__TOOLS_TAGGED_UNION_TEST_NEGATIVE_H_
#define __ELFS__TOOLS_TAGGED_UNION_TEST_NEGATIVE_H_

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


#endif
