#!/bin/bash

VERIFY="false"

RUN_NUM=20
TEST_FILE="image4.bmp"
FILTERS=( "co" "sh" "bb" "gb" "em" "mb" "mg" "gg" "bo") # mm can be added, but has too high execution time (x20)
BLOCK_SIZE=("4" "8" "16" "32" "64" "128")
THREADNUM=4
MODES=("by_row" "by_column" "by_grid") # removed by_pixel due to too big execution time
FILTER_PAIRS="gb,sh sh,gb mb,sh sh,mb gg,sh sh,gg"

SD=$(dirname "$(realpath "$0")") # script directory
BASEDIR=$(dirname "$SD")  # Parent directory of script's location
BD="$BASEDIR"
IMG_FOLDER="$BD/test-img/"
LOG_FILE="$SD/timing-results.dat"
PLOTS_PATH="$SD/plots/"

IFS=' ' read -r -a pairs <<< "$FILTER_PAIRS"

for pair in "${pairs[@]}"; do
    IFS=',' read -r f1 f2 <<< "$pair"
    echo "f1: $f1, f2: $f2"
done

compare_results() {
    filename=$1
    if diff -q "${IMG_FOLDER}seq_out_${filename}" "${IMG_FOLDER}rcon_out_${filename}" > /dev/null; then
        echo -e "Files are identical\n"
    else
        echo -e "Files differ\n"
        diff "${IMG_FOLDER}seq_out_${filename}" "${IMG_FOLDER}rcon_out_${filename}"
        exit 1
    fi
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -v|--verify) VERIFY="true" ;;
        -h|--help) echo "Usage: $0 [-v|--verify]"; exit 1 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
    shift
done


make -C "$BD" clean
make -C "$BD" build

if [ "$VERIFY" == "false" ]; then 
	echo "RunID Filter-type Thread-num Mode Block-size Result" > "$LOG_FILE"

	mkdir -p "$PLOTS_PATH" "$PLOTS_PATH/mt" "$PLOTS_PATH/st"

	echo -e "\nRunning single-threaded tests"
	for fil in "${FILTERS[@]}"; do
		make -C "$BD" run-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM="$THREADNUM" LOG=0

		for i in $(seq 1 "$RUN_NUM"); do
			echo -n "$i " >> "$LOG_FILE"
			make -C "$BD" run-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=1 

			compare_results "$TEST_FILE"
		done
	done

	python3 "$SD/avg_plots.py"

	echo -e "\nRunning multithreaded tests"
	for mode in "${MODES[@]}"; do
		for fil in "${FILTERS[@]}"; do
			for bs in "${BLOCK_SIZE[@]}"; do
				make -C "$BD" run-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=1  LOG=0

				for i in $(seq 1 "$RUN_NUM"); do
					echo -n "$i " >> "$LOG_FILE"
					make -C "$BD" run-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM="$THREADNUM" BLOCK_SIZE="$bs" COMPUTE_MODE="$mode"
					compare_results "$TEST_FILE"
				done
			done
		done
	done

	python3 "$SD/avg_plots.py"

	echo -e "\nRunning multithreaded tests with filter composition"
	for pair in "${pairs[@]}"; do
		IFS=',' read -r f1 f2 <<< "$pair"
		echo -e "SEQUENTIAL MODE:\n"

		make -C "$BD" run-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$f1" THREADNUM=1 OUTPUT_FILE="${f1}_$TEST_FILE" LOG=0 
		make -C "$BD" run-p-cores INPUT_TF="${f1}_$TEST_FILE" FILTER_TYPE="$f2" THREADNUM=1 OUTPUT_FILE="seq_out_${f1}_${f2}_$TEST_FILE" LOG=0 

		for i in $(seq 1 "$RUN_NUM"); do
			echo -n "$i " >> "$LOG_FILE"
			echo -e "MULTITHREADED MODE:\n"

			make -C "$BD" run-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$f1" THREADNUM="$THREADNUM" BLOCK_SIZE=16 COMPUTE_MODE=by_grid OUTPUT_FILE="${f1}_$TEST_FILE"
			echo -n "$i " >> "$LOG_FILE"
			make -C "$BD" run-p-cores INPUT_TF="${f1}_$TEST_FILE" FILTER_TYPE="$f2" THREADNUM="$THREADNUM" BLOCK_SIZE=16 COMPUTE_MODE=by_grid OUTPUT_FILE="rcon_out_${f1}_${f2}_$TEST_FILE"

			compare_results "${f1}_${f2}_$TEST_FILE"
		done
	done

	python3 "$SD/comp-plots.py"

else 
	echo -e "\nRunning single-threaded verification tests"
	for fil in "${FILTERS[@]}"; do
		make -C "$BD" run-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM="$THREADNUM" LOG=0 > /dev/null

			make -C "$BD" run-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=1 LOG=0 > /dev/null 
			compare_results "$TEST_FILE"
	done


	echo -e "\nRunning multithreaded verification tests"
	for mode in "${MODES[@]}"; do
		for fil in "${FILTERS[@]}"; do
			for bs in "${BLOCK_SIZE[@]}"; do
				make -C "$BD" run-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=1 LOG=0 >/dev/null

					make -C "$BD" run-p-cores INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM="$THREADNUM" BLOCK_SIZE="$bs" COMPUTE_MODE="$mode" LOG=0 > /dev/null
					compare_results "$TEST_FILE"
			done
		done
	done
fi
