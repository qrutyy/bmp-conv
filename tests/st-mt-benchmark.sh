#!/bin/bash

SD=$(dirname "$(realpath "$0")")
BASEDIR=$(dirname "$SD")
BD="$BASEDIR"
BUILD_DIR="${BUILD_DIR:-$BD/build}"

LOG_FILE="$SD/logs/cpu-timing-results.dat"
PLOTS_PATH="$SD/plots/"
RUN_NUM=2
UNIFIED_HEADER="RunID ProcessNum Backend Mode Filter ThreadNum ComputeMode BlockSize Result"

TEST_FILE="image5.bmp"
FILTERS=(co sh bb gb em mb mg gg bo)
BLOCK_SIZE=(4 8 16 32 64 128)
THREADNUM=4
MODES=("by_row" "by_column" "by_grid")
FILTER_PAIRS="gb,sh sh,gb mb,sh sh,mb gg,sh sh,gg"

IFS=' ' read -r -a pairs <<< "$FILTER_PAIRS"

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake -B "$BUILD_DIR"
fi
cmake --build "$BUILD_DIR"

BIN="$BUILD_DIR/bmp-conv"
if [[ ! -x "$BIN" ]]; then
    echo "Error: executable not found: $BIN"
    exit 1
fi

cd "$BD" || exit 1
mkdir -p "$SD/logs" "$PLOTS_PATH" "$PLOTS_PATH/mt" "$PLOTS_PATH/st"
echo "$UNIFIED_HEADER" > "$LOG_FILE"

echo -e "\nRunning single-threaded tests"
for fil in "${FILTERS[@]}"; do
	for i in $(seq 1 "$RUN_NUM"); do
		echo -n "$i 1 " >> "$LOG_FILE"
		"$BIN" -cpu "$TEST_FILE" --filter="$fil" --mode=by_row --block=1 --threadnum=1 --log=1
	done
done

python3 "$SD/avg-plots.py"

echo -e "\nRunning multithreaded tests"
for mode in "${MODES[@]}"; do
	for fil in "${FILTERS[@]}"; do
		for bs in "${BLOCK_SIZE[@]}"; do
			for i in $(seq 1 "$RUN_NUM"); do
				echo -n "$i 1 " >> "$LOG_FILE"
				"$BIN" -cpu "$TEST_FILE" --filter="$fil" --threadnum="$THREADNUM" --block="$bs" --mode="$mode" --log=1
			done
		done
	done
done

python3 "$SD/avg-plots.py"

echo -e "\nRunning multithreaded tests with filter composition"
for pair in "${pairs[@]}"; do
	IFS=',' read -r f1 f2 <<< "$pair"
	echo "Filter pair: $f1 -> $f2"

	"$BIN" -cpu "$TEST_FILE" --filter="$f1" --threadnum=1 --mode=by_row --block=16 --log=0 --output="${f1}_$TEST_FILE"
	"$BIN" -cpu "${f1}_$TEST_FILE" --filter="$f2" --threadnum=1 --mode=by_row --block=16 --log=0 --output="seq_out_${f1}_${f2}_$TEST_FILE"

	for i in $(seq 1 "$RUN_NUM"); do
		"$BIN" -cpu "$TEST_FILE" --filter="$f1" --threadnum="$THREADNUM" --block=16 --mode=by_grid --log=0 --output="${f1}_$TEST_FILE"
		"$BIN" -cpu "${f1}_$TEST_FILE" --filter="$f2" --threadnum="$THREADNUM" --block=16 --mode=by_grid --log=0 --output="rcon_out_${f1}_${f2}_$TEST_FILE"
	done
done

python3 "$SD/comp-plots.py"
