# elfs-clang-plugins

A collection of clang plugins for safer C programming.

* enums_conversion: Finds implicit casts to/from enums and integral types
* include_cleaner: Finds unused #includes
* large_assignment: Finds copies or initializations which may result is large copies (size is configurable)
* private: Assuming a certain convention, finds access to fields of structs that are not explicitly exposed in an _api.h file.

See below for more information about each plugin.

Also, each plugin has a `test` directory with both positive and negative test cases, which also serve as good examples.

# Usage

Requires llvm-config-64 to be installed.

Build & test:

    make
    make test

See `CFLAG_CLANG_PLUGINS` in plugins.mk for example command line argument for using the plugins.

# Plugins

## enums_conversion

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

## include_cleaner

The include_cleaner plugin helps you get rid of unneccessary includes, for example:

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
