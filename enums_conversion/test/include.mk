.=enums_conversion/test

default: $./default
clean: $./clean

$./out.fail/%.enum_conversion_error: $./out.fail/%.o
	./fail_expecting.sh "$<".fail "$@" "enum conversion"

$./buildonly: $./out/positive.o \
    $./out.fail/negative_arrow.enum_conversion_error \
    $./out.fail/negative_bad_conditional.enum_conversion_error \
    $./out.fail/negative_bitwise_or.enum_conversion_error \
    $./out.fail/negative_compare.enum_conversion_error \
    $./out.fail/negative_compare_2.enum_conversion_error \
    $./out.fail/negative_literal_int.enum_conversion_error \
    $./out.fail/negative_member.enum_conversion_error \
    $./out.fail/negative_param.enum_conversion_error \
    $./out.fail/negative_var.enum_conversion_error \
    $./out.fail/negative_if.enum_conversion_error \
    $./out.fail/negative_return.enum_conversion_error \
    $./out.fail/negative_return_2.enum_conversion_error \
    $./out.fail/negative_assign.enum_conversion_error \
    $./out.fail/negative_assign_2.enum_conversion_error \
    $./out.fail/negative_var_decl.enum_conversion_error \
    $./out.fail/negative_var_decl_2.enum_conversion_error \
    $./out.fail/negative_not.enum_conversion_error \
    $./out.fail/negative_switch.enum_conversion_error


$./extra_clean:
	rm -f $./out.fail/*.enum_conversion_error

include $./../../test.mk
