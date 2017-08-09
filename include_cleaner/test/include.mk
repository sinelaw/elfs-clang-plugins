.=include_cleaner/test

default: $./default
clean: $./clean

$./out.fail/%.include_cleaner_error: $./out.fail/%.o
	./fail_expecting.sh "$<".fail "$@" "include cleaner"

$./buildonly: $./out/positive.o \
    $./out.fail/negative_redundant_allow.include_cleaner_error \
    $./out.fail/negative_simple.include_cleaner_error \
    $./out.fail/negative_var.include_cleaner_error
#    $./out.fail/negative_non_extern_func.include_cleaner_error \

$./extra_clean:
	rm -f $./out.fail/*.include_cleaner_error

include $./../../test.mk
