include plugins.mk

MKDIR=mkdir -p
CC=/usr/bin/clang-5.0 -Werror -Wall -Wextra -g ${CFLAG_CLANG_PLUGINS}


.PHONY: $./buildonly $./default

$./default: $./buildonly

$./out:
	${MKDIR} "$@"
$./out.fail:
	${MKDIR} "$@"

$./out.fail/%.o $./out.fail/%.o.fail: $./%.c clang_plugins.so $./out.fail test.mk plugins.mk
	./fail.sh "${CC} -c $< -o $./out.fail/$(@F)" $./out.fail/$(@F)

$./out/%.o: $./%.c clang_plugins.so $./out test.mk plugins.mk
	${CC} $< -c -o $@

$./clean: $./extra_clean
	rm -f $./out/*.o $./out.fail/*.o $./out.fail/*.fail

