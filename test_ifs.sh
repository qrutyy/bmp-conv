#!/bin/bash
ARR=("a" "b" "c")
OUT=$(IFS=";"; echo "${ARR[*]}")
echo "With subshell assignment: $OUT"

IFS=";"
OUT2="${ARR[*]}"
echo "With global IFS: $OUT2"
