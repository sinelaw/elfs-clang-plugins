# Build C++ code that uses the clang libraries, and thus need the LLVM config flags
CXX_WITH_LLVM_CONFIG=\
    clang++-5.0 `llvm-config-5.0 --cxxflags` -g "$<" -o "$@" -c 

CXX_LINK_WITH_LLVM_CONFIG=\
    clang++-5.0 `llvm-config-5.0 --ldflags` -g \
    $^ -o "$@" -lffi -lm  -ltinfo -lpthread -ldl

MKDIR=mkdir -p

.PHONY: default clean tests

default: clang_plugins.so

PLUGINS={large_assignment,tagged_union,include_cleaner,private,enums_conversion,warn_unused_result}

tests: clang_plugins.so
	make -f enums_conversion/test/include.mk
	make -f include_cleaner/test/include.mk
	make -f large_assignment/test/include.mk
	make -f warn_unused_result/test/include.mk

clean:
	make -f enums_conversion/test/include.mk clean
	make -f include_cleaner/test/include.mk clean
	make -f large_assignment/test/include.mk clean
	make -f warn_unused_result/test/include.mk clean
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

warn_unused_result.o: warn_unused_result/warn_unused_result.cpp
	${CXX_WITH_LLVM_CONFIG}

# tagged_union.o
clang_plugins.so: enums_conversion.o large_assignment.o include_cleaner.o private.o warn_unused_result.o
	${CXX_LINK_WITH_LLVM_CONFIG} -fpic -shared -lclangASTMatchers
