#!/bin/bash

SD=$(dirname "$(realpath "$0")")   # script directory (tests/)
BASEDIR=$(dirname "$SD")          # project root
BD="$BASEDIR"
BUILD_DIR="${BUILD_DIR:-$BD/build}"

LOG_FILE="$SD/logs/gpu-timing-results.dat"
PLOTS_PATH="$SD/plots/"
RUN_NUM=25
UNIFIED_HEADER="RunID ProcessNum Backend Mode Filter ThreadNum ComputeMode BlockSize Result"

TEST_FILE=""
FILTERS=("co" "sh" "bb" "gb" "em" "mb" "mg" "gg" "bo") # mm can be added, but has too high execution time (x20)
BLOCK_SIZE_GPU=(1 4 8 16 32)

# Build with cmake (configure if needed)
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
	cmake -B "$BUILD_DIR" -DENABLE_OPENCL=ON
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

if [[ ! -e "$TEST_FILE" ]]; then
	TEST_FILE="checker.bmp"
	convert -size 16000x16000 pattern:checkerboard test-img/checker.bmp
fi

echo -e "\nRunning GPU tests (block size = work group size)"
for fil in "${FILTERS[@]}"; do
	for bs in "${BLOCK_SIZE_GPU[@]}"; do
		for i in $(seq 1 "$RUN_NUM"); do
			echo -n "$i 1 " >> "$LOG_FILE"
			"$BIN" -gpu "$TEST_FILE" --filter="$fil" --mode=by_row --block="$bs" --log=1
		done
	done
done

python3 "$SD/gpu-plots.py"
