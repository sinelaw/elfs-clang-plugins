# elfs-clang-plugins

A collection of clang plugins for safer C programming. The plugins detect problems at compile time.

* enums_conversion: Finds implicit casts to/from enums and integral types
* `warn_unused_result`: Warns about functions declared without the `warn_unused_result` attribute
* include_cleaner: Finds unused #includes
* large_assignment: Finds large copies in assignments and initializations (size is configurable)
* private: Prevents access to fields of structs that are defined in private.h files

See below for more information about each plugin.

Each plugin has a `test` directory with both positive and negative test cases, which also serve as good examples.

# Usage

Building the plugin requires llvm/clang development libraries to be installed, it expects an `llvm-config-64` to be available.

On debian or ubuntu, try installing: llvm-3.7-dev clang-3.7 libclang-3.7-dev. You may need to use a PPA.

Tested also on Centos 7.1, using the packages: llvm-static, llvm-libs, llvm-devel, clang-devel. You may need to get them from copr.

Once you have the prerequisites, build & test:

    make
    make tests

All the plugins are linked into a single .so file, for convenience, but are controlled individually via clang command
line arguments.

Example usage (once `clang_plugins.so` is built):

    clang -Xclang -load -Xclang ./clang_plugins.so \
          -Xclang -add-plugin -Xclang include_cleaner \
          -Xclang -add-plugin -Xclang enums_conversion \
          -Xclang -add-plugin -Xclang large_assignment -Xclang -plugin-arg-large_assignment -Xclang 700 \
          -c my_file.c -o my_file.o

See `CFLAG_CLANG_PLUGINS` in `plugins.mk` for the full command line argument for using all plugins.

# Plugins

## enums_conversion

(Check [enums_conversion/README.md](enums_conversion/README.md) for more information about the enums_conversion plugin.)

In C, enum types are implicitly convertible to other integral types. The following code compiles without warnings, even
with `-Wall -Wextra` (tested on GCC 6.3 & clang 3.7):

    enum Result { OK, NO_BUNNIES };

    enum Result get_bunnies(void) {
        return 3;
    }

The enums_conversion plugin prevents such silly mistakes:

    clang -Xclang -load -Xclang ./clang_plugins.so -Xclang -add-plugin -Xclang enums_conversion -c /tmp/test.c
    /tmp/test.c:7:12: error: enum conversion to or from enum Result
        return 3;
               ^
    1 error generated.

Among other cases detected by the plugin:

* Passing, assigning or returning a value of the wrong type when an enum is expected
* Using enums as bool in conditional expressions
* Using values of wrong type in enum flag bitwise or `|` expressions
* etc.

There are cases (or styles of C coding) that use enums as actual numbers (such as array indices). In those cases you can
use an explicit cast to make the fact that the enum needs to be converted, well, explicit.

Note: The enums_conversion plugin uses AST matching to find violations - it works by looking for specific problematic
patterns. A more complete approach would be to traverse the entire AST and perform a complete type-checking pass (or to
augment the existing clang type checker, but as far as I can tell, a clang plugin can't do that). However, we found the
AST matching approach to be quite effective, and it was easy to implement.

## `warn_unused_result`

Both clang and gcc support the attribute `warn_unused_result` on functions, but provide no way to treat all functions as if they have the attribute, nor is there a way to warn about functions that don't have the attribute.

The `warn_unused_result` plugin will emit a warning functions that **lack a `warn_unused_result` attribute**. The plugin only considers functions declared or defined in the current compilation unit.

The plugin accepts an optional arg, `--static-only` which causes it to warn only about static functions (for when changing external APIs is too much work).

Example:

    int foo(void);

Compiler output:

    /tmp/test.c:1:5: warning: missing attribute warn_unused_result
    int foo(void);

## include_cleaner

The include_cleaner plugin helps you get rid of unnecessary includes, for example:

foo.h:

    struct Foo { };

bar.c:

    #include "foo.h"

    // never uses anything from foo.h

In this case, the plugin will give an error:

    clang -Xclang -load -Xclang ./clang_plugins.so -Xclang -add-plugin -Xclang include_cleaner -c /tmp/bar.c
    /tmp/bar.c:1:1: warning: include cleaner: unused #include of '/tmp/foo.h'
    #include "foo.h"
    ^
    1 warning generated.

The plugin expects your code to follow a certain pattern:

1. `foo.c` is always allowed to #include `foo.h` or `foo_api.h`
2. External files are always allowed to be #included (currently just anything in `/usr`)
3. Any other file that's #included must be used, by referencing a type, function or global defined directly in that file.

Note that this prohibits "mega-h-files", which are .h files that just include other .h files in order to expose them.

Our intention with this plugin is to enforce #including **only directly what you need**. It makes dependencies easier to
understand. Also, the compiler doesn't need to work on all those unused files - in one case we saved >30% compilation
time in a large project by just cleaning up our #includes.

Files from `/usr` are always allowed because they don't follow any obvious pattern of who is responsible for
what. `<errno.h>` doesn't actually define `errno` - it does a bunch of nested includes which eventually bring in the
global variable.


## large_assignment

Consider the following code:

    struct Context {
        char scratch_buffer[80*1024];
        int field;
    };

    void init_field(struct Context *ctx, int x) {
        *ctx = (struct Context) { /* 80 kb copy hiding here */
            .field = x,
        };
    }

The large_assignment plugin helps to find assignments which look innocent but may incur a big copy. The size is
configurable via a clang plugin command line argument.

With the plugin, the above example yields:

    clang -Wall -Wextra -g -Xclang -load -Xclang ./clang_plugins.so \
        -Xclang -add-plugin -Xclang large_assignment \
        -Xclang -plugin-arg-large_assignment -Xclang 1024 \
        -c /tmp/large.c -o /tmp/large.o
    /tmp/large.c:7:16: warning: large assignment of 81924 bytes is more than allowed 1024 bytes
            *ctx = (struct Context) {
                   ^
    1 warning generated.

To change the maximum allowed, use the command line arguments, e.g.
`-Xclang -plugin-arg-large_assignment -Xclang 1024`

# private

The `private` plugin enforces a coding convention, where struct fields may not be accessed directly unless they are
exposed in a public .h file.

The plugin assumes that private .h files are suffixed with `_private.h`

The rule is: `module.c` is allowed to access privates from `module_private.h`, but not from any other private file
(e.g. stuff from `other_private.h` would not be allowed).

Example project:

foo.h:

    #include "foo_private.h"

    typedef struct _Foo Foo;

    /* Public API, defined in foo.c, supposedly (implementation doesn't matter for the example) */
    void foo_init(Foo *);
    void foo_set_value(Foo *, int);

foo_private.h:

    struct _Foo {
        int field; /* supposed to be private, accessible via API function */
    };

usage.c:

    #include "foo.h"

    void bar(Foo *foo) {

        foo->field = 3; /* BAD! should use API foo_set_value() */

    }


The plugin complains about this API circumvention:

    clang -Wall -Wextra -g -Xclang -load -Xclang ./clang_plugins.so -Xclang -add-plugin -Xclang private  -c /tmp/usage.c -o /tmp/usage.o
    /tmp/usage.c:5:14: warning: access to private member of _Foo
            foo->field = 3; /* BAD! should use API foo_set_value() */
                 ^
    /tmp/foo_private.h:1:12: note: declaration of _Foo
        struct _Foo {
               ^
    1 warning generated.

If a struct's internals are supposed to be exposed, you just need to define it in the non-private .h file.

You sometimes want to allow accessing a private field. Common examples are tests, or when a module is deliberately split
into several .c files all sharing the same `_private.h`. For those cases there is `FRIEND_OF`, a macro in `private_api.h`:

    /* FRIEND_OF takes a single *type name* as an argument */
    FRIEND_OF(Foo); /* Put this a file level. Now we are allowed in this .c file to access internals of Foo. */

The idea is not to abuse `FRIEND_OF` too much.
