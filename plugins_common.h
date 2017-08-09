#pragma once

#define CONCAT(A,B) _CONCAT(A,B)
#define _CONCAT(A, B) A ## B

#define STR(s) _STR(s)
#define _STR(s) #s


#define UNUSED(x) CONCAT(x, CONCAT(_unused_, __COUNTER___)) __attribute__((unused))

