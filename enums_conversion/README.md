# Type-safe enums in C

The C programming language generally treats enums as integers (see "Appendix: For Language Lawyers" for reference).

Wouldn't it be nice if we could do more with enums, and do it safely?

Some other languages have anything from integer-incompatible enums to full-blown sum types. It would be nice to have
something like that in C.

The enums_conversion clang plugin aims to do just that, by treating enums as incompatible with integers (except via
explicit casting).

## A motivating example

Some people are surprised at the goals of this plugin. Here is a simple example to explain the motivation.

Consider the following (totally fabricated) API:

    enum OpGetResult {
        OP_GET_ERROR,
        OP_GET_OK,
    };

    enum OpGetResult get_items(void);

    /* Implementation: */

    enum OpGetResult get_items(void)
    {
        /* ... do something with side effects ... */
        return OP_GET_OK;
    }

So far so good. Safe it as `test.c` and compile this program with gcc:

    gcc -std=c11 -Wall -Wextra -Werror -c test.c

No errors, yay!

### A simple bug

Now, let's introduce a bug. Someone decided the API is no good, and `get_items` should just return the number of items
it "got". So the new API is:

    /* This enum is in use elsewhere... */
    enum OpGetResult {
        OP_GET_ERROR,
        OP_GET_OK,
    };

    int get_items(void); /* return value changed to 'int' */

    /* Implementation: */

    int get_items(void)
    {
        /* ... do something with side effects ... */
        return OP_GET_OK; /* oops! forgot to change this */
    }

The bug is that `get_items` still returns the enum value `OP_GET_OK` instead of a number.

Save as test2.c and compile (tested on gcc 6.3.0):

    gcc -std=c11 -Wall -Wextra -Werror -c test2.c

Oh no! No error! Let's try with clang 5.0 and the wonderful `-Weverything` which enables all warnings:

    clang -std=c11 -Weverything -Werror  -c test2.c

Nope! Still no error.

The compilers are ok with this code *because it's allowed*. However, it's clearly not what we intended.

### A bunch of other possible bugs

Here is a snippet with different 'bad code' examples: (for testing it can be appended to one of the previous files)

    int func(enum OpGetResult e, unsigned int x, unsigned int y);
    int func(enum OpGetResult e, unsigned int x, unsigned int y)
    {
      handle_result(x); /* passing arbitrary integer where one of several enum values was expected */

      enum OpGetResult e2 = x; /* assigning from arbitrary integer (which may not be a valid enum value) */

      if (e2 == y) { /* comparing enum to arbitrary integer */
      }

      return e; /* returning enum where arbitrary integer is expected by caller */
    }

Neither  gcc 6.3.0 nor clang 5.0 emit any kind of warning about the above code.

    # Let's try gcc with some extra warnings:
    gcc -std=c11 -Wall -Wextra -Werror -Wconversion -Wenum-compare -Wswitch-enum -Wsign-conversion  -c test2.c

    # clang with -Weverything:
    clang -std=c11 -Weverything -Werror  -c test2.c

### clang plugin to the rescue

The `enums_converesion` clang plugin detects and warns about all of the above.

    # clang -std=c11 -Weverything  -c test2.c -Xclang -load -Xclang ./clang_plugins.so -Xclang -add-plugin -Xclang enums_conversion
    test2.c:22:23: error: enum conversion to or from enum OpGetResult
            handle_result(x); /* passing arbitrary integer where one of several enum values was expected */
                          ^
    test2.c:24:31: error: enum conversion to or from enum OpGetResult
            enum OpGetResult e2 = x; /* assigning from arbitrary integer (which may not be a valid enum value) */
                                  ^
    test2.c:26:13: error: enum conversion to or from enum OpGetResult
            if (e2 == y) { /* comparing enum to arbitrary integer */
                ^
    test2.c:29:16: error: enum conversion to or from enum OpGetResult
            return e; /* returning enum where arbitrary integer is expected by caller */
                   ^
    4 errors generated.

## Frequently Asked Questions

1. But this isn't standard C!

Correct, it is a *restrictive subset* of C. Some "valid" C programs will be flagged by this plugin. I believe writing
code in the spirit of this plugin will improve your code's readability while preventing a class of bugs from ever occurring.

2. How is this different from gcc's `-Wenum-compare`?

The warning flag `-Wenum-compare` find comparisons between different enums, but does not look at comparing enums to
integers, implicit casting to/from integers, etc. In the following program only the second `if` is flagged by `-Wenum-compare`:

    enum A { A_FIRST, A_SECOND };
    enum B { B_FIRST, B_SECOND };

    int foo(enum A a, unsigned int x);
    int foo(enum A a, unsigned int x) {
          if (x == a) { // no warning emitted
              return 1;
          }
          if (B_FIRST == a) { // will cause warning: comparison between ‘enum B’ and ‘enum A’
              return 2;
          }
          return 0;
    }

3. How is this different from clang's `-Wenum-conversion`?

`-Wenum-conversion` doesn't catch implicit casts to/from integral types (the plugin does).

`-Wenum-conversion` does catch conversion from one enum type to another, like so:

    enum EnumA { E_A };
    enum EnumB { E_B };
    enum EnumA do_something(void) {
        return E_B;
    }

4. What about enums being used as combinable bits? Won't the plugin disallow them?

A common pattern is using an enum to describe the allowed bits for an "options" value that can be ORed together. For example:

    enum Flags {
        FLAG_NONE = 0,
        FLAG_READ = 1,
        FLAG_WRITE = 2,
    };
    enum Flags do_something(void);
    enum Flags do_something(void) {
        return FLAG_WRITE | FLAG_READ;
    }

The plugin is OK with this. clang -Weverything doesn't like this (-Wassign-enum):

    clang -std=c11 -c /tmp/test.c -Weverything
    /tmp/test.c:8:12: warning: integer constant not in range of enumerated type 'enum Flags' [-Wassign-enum]
        return FLAG_WRITE | FLAG_READ;
               ^
    1 warning generated.

That's a false error (if you use | with a runtime variable, `-Wassign-enum` seems to not flag this).
However, the plugin does catch errors of putting an invalid value in the OR expression:

    ...
    return FLAG_WRITE | 5;

Now clang -Weverything doesn't complain (despite the possible bug).

Running with the plugin gives:

    /tmp/test.c:10:16: error: enum conversion to or from enum Flags
            return FLAG_WRITE | 5;

5. I'm afraid to use this in production.

The plugin only analyzes the AST produced by clang, and does not affect the emitted code in any way.

6. I don't use clang! Can I benefit from this plugin?

At [elastifile](http://elastifile.com/), the plugin is being used as part of the CI process. Code that is being merged
into master must pass the plugin's checks (as well as other plugins from this suite). The actual production executable
is built by gcc (for various unrelated reasons).


## Appendix: For Language Lawyers

The [C11 standard (draft)](http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) says:

> An enumeration comprises a set of named integer constant values. Each distinct enumeration constitutes a different enumerated type.
> The type char, the signed and unsigned integer types, and the enumerated types are collectively called integer types...
