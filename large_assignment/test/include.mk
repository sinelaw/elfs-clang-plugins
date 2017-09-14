.=large_assignment/test

default: $./default
clean: $./clean

$./out.fail/%.large_assignment_error: $./out.fail/%.o
	./fail_expecting.sh "$<".fail "$@" "large assignment"

$./buildonly: $./out/positive.o \
    $./out.fail/negative_simple.large_assignment_error \
    $./out.fail/negative_not_init.large_assignment_error \
    $./out.fail/negative_call.large_assignment_error

$./extra_clean:
	rm -f $./out.fail/*.large_assignment_error

include $./../../test.mk
