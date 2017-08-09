#!/bin/bash
set -eu
COMMAND="$1"
OUTPUT="$2"
if ( ! $COMMAND ) &> "$OUTPUT.fail" ; then
    echo "failed as expected" >> "$OUTPUT"
else
    echo "Should fail but didn't! $<" 1>&2
    exit 1
fi
