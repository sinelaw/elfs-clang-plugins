# Build C++ code that uses the clang libraries, and thus need the LLVM config flags
CXX_WITH_LLVM_CONFIG=\
    g++ ${CFLAGS_GCC} `llvm-config-64 --cxxflags` -O2 -g "$<" -o "$@" -c -Wall -Wextra -Wno-unused -Wno-strict-aliasing -Werror

CXX_LINK_WITH_LLVM_CONFIG=\
    g++ ${LDFLAGS_GCC} `llvm-config-64 --ldflags` -O2 -g \
    $^ -o "$@" -lffi -lm  -ltinfo -lz -lpthread -ldl

MKDIR=mkdir -p

.PHONY: default clean tests

default: clang_plugins.so

PLUGINS={large_assignment,tagged_union,include_cleaner,private,enums_conversion}

tests: clang_plugins.so
	make -f enums_conversion/test/include.mk
	make -f include_cleaner/test/include.mk
	make -f large_assignment/test/include.mk

clean:
	make -f enums_conversion/test/include.mk clean
	make -f include_cleaner/test/include.mk clean
	make -f large_assignment/test/include.mk clean
	rm -f *.o clang_plugins.so

enums_conversion.o: enums_conversion/enums_conversion.cpp
	${CXX_WITH_LLVM_CONFIG}

large_assignment.o: large_assignment/large_assignment.cpp
	${CXX_WITH_LLVM_CONFIG}

tagged_union.o: tagged_union/tagged_union.cpp
	${CXX_WITH_LLVM_CONFIG}

include_cleaner.o: include_cleaner/include_cleaner.cpp
	${CXX_WITH_LLVM_CONFIG}

private.o: private/private.cpp
	${CXX_WITH_LLVM_CONFIG}

# tagged_union.o
clang_plugins.so: enums_conversion.o large_assignment.o include_cleaner.o private.o
	${CXX_LINK_WITH_LLVM_CONFIG} -fpic -shared -lclangASTMatchers

