.=warn_unused_result/test

default: $./default
clean: $./clean

$./out.fail/%.warn_unused_result_error: $./out.fail/%.o
	./fail_expecting.sh "$<".fail "$@" "warn_unused_result"

$./buildonly: $./out/positive.o \
    $./out.fail/negative_simple.warn_unused_result_error


$./extra_clean:
	rm -f $./out.fail/*.warn_unused_result_error

include $./../../test.mk
