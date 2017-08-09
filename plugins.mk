LARGE_ASSIGNMENT_MAX_ALLOWED_BYTES=1024 # for example


CLANG_PLUGINS_SO=clang_plugins.so
CCOMPILE_DEPS_CLANG_PLUGINS=${CLANG_PLUGINS_SO}
CFLAG_PLUGIN_LARGE_ASSIGNMENT=-Xclang -add-plugin -Xclang large_assignment \
    -Xclang -plugin-arg-large_assignment -Xclang ${LARGE_ASSIGNMENT_MAX_ALLOWED_BYTES}
CFLAG_PLUGIN_ENUMS_CONVERSION=-Xclang -add-plugin -Xclang enums_conversion -DCLANG_PLUGIN_ENUMS_CONVERSION
CFLAG_PLUGIN_TAGGED_UNION=-Xclang -add-plugin -Xclang tagged_union
CFLAG_PLUGIN_PRIVATE=-Xclang -add-plugin -Xclang private
CFLAG_PLUGIN_INCLUDE_CLEANER=-Xclang -add-plugin -Xclang include_cleaner
# example whitelist:
#    -Xclang -plugin-arg-include_cleaner -Xclang debug/edeb/edeb_user_remote_functions.h
CFLAG_CLANG_PLUGINS=-Xclang -load -Xclang ./${CLANG_PLUGINS_SO} ${CFLAG_PLUGIN_LARGE_ASSIGNMENT} ${CFLAG_PLUGIN_ENUMS_CONVERSION} ${CFLAG_PLUGIN_INCLUDE_CLEANER} ${CFLAG_PLUGIN_PRIVATE}
