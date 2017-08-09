# elfs-clang-plugins

A collection of clang plugins for safer C programming.

* enums_conversion: Finds implicit casts to/from enums and integral types
* include_cleaner: Finds unused #includes
* large_assignment: Finds copies or initializations which may result is large copies (size is configurable)
* private: Assuming a certain convention, finds access to fields of structs that are not explicitly exposed in an _api.h file.

# Usage

Requires llvm-config-64 to be installed.

Build & test:

    make
    make test

See `CFLAG_CLANG_PLUGINS` in plugins.mk for example command line argument for using the plugins.
