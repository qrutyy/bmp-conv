#!/bin/bash

SD=$(dirname "$(realpath "$0")") # script directory
BASEDIR=$(dirname "$SD")  # Parent directory of script's location
BD="$BASEDIR"

THREADNUM=4
MODES=("by_row" "by_column" "by_grid") # removed by_pixel due to too big execution time
FILTERS=( "co" "sh" "bb" "gb" "em" "mb" "mg" "gg" "bo") # mm can be added, but has too high execution time (x20)
TEST_FILE="image5.bmp"
BLOCK_SIZE=("4" "8" "16" "32" "64" "128")

make -C "$BD" clean
make -C "$BD" build

echo -e "\nRunning single-threaded verification tests"
	for fil in "${FILTERS[@]}"; do
		make -C "$BD" run-mac-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM="$THREADNUM" LOG=0 > /dev/null
		make -C "$BD" run-mac-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=1 LOG=0 > /dev/null 
		compare_results "$TEST_FILE"
	done

	echo -e "\nRunning multithreaded verification tests"
	for mode in "${MODES[@]}"; do
		for fil in "${FILTERS[@]}"; do
			for bs in "${BLOCK_SIZE[@]}"; do
				make -C "$BD" run-mac-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=1 LOG=0 >/dev/null
				make -C "$BD" run-mac-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM="$THREADNUM" BLOCK_SIZE="$bs" COMPUTE_MODE="$mode" LOG=0 > /dev/null
				compare_results "$TEST_FILE"
			done
		done
	done

