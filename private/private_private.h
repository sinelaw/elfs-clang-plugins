#define PRIVATE__CONCAT_(A, B) A ## B
#define PRIVATE__CONCAT(A,B) PRIVATE__CONCAT_(A,B)

#define PRIVATE_VAR_PREFIX private__friend_

#define PRIVATE_FRIEND_OF(type) \
    static const type *PRIVATE__CONCAT(PRIVATE_VAR_PREFIX, __COUNTER__) __attribute__((used)) \
        = ((const type*)0);
