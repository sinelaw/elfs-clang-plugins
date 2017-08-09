#!/bin/bash
set -eu
INPUT="$1"
shift
OUTPUT="$1"
shift
PATTERN="$@"
grep "$PATTERN" "$INPUT" > "$OUTPUT"
if [ "`cat ${OUTPUT} | wc -l`" -ne "1" ]; then
    echo "Expected exactly one $PATTERN error! Got:"
    cat "$INPUT"
    exit 1
fi
