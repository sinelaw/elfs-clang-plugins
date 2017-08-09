local {

.=tools/clang_plugins/private/test

NEGATIVES={simple}

$./out.fail/%.private_error: $./out.fail/%.fail
	grep 'access to private member' "$<" > "$@"
	if [ "`cat $@ | wc -l`" -ne "1" ]; then
	    echo 'Expected exactly one "private" error! Got:'
	    cat "$<"
	    exit 1
	fi

# TODO: remove this custom build rules when plugin is enabled by default in Common.mk
ifeq (${FLAG_clang_plugins},enable)
CFLAG_PLUGIN_PRIVATE=-Xclang -add-plugin -Xclang private
endif

include /build/defaults.mk

ifeq (${FLAG_clang_plugins},enable)
$./buildonly: $./out/positive.o $./out.fail/negative_${NEGATIVES}.private_error
else
$./buildonly: $./out/positive.o $./out/negative_${NEGATIVES}.o
endif
$./runtests:

local }
