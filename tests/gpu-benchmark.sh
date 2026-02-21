#!/bin/bash

SD=$(dirname "$(realpath "$0")")   # script directory (tests/)
BASEDIR=$(dirname "$SD")          # project root
BD="$BASEDIR"
BUILD_DIR="${BUILD_DIR:-$BD/build}"

LOG_FILE="$SD/timing-results.dat"
PLOTS_PATH="$SD/plots/"
RUN_NUM=25

TEST_FILE="image-7.bmp"
FILTERS=("co" "sh" "bb" "gb" "em" "mb" "mg" "gg" "bo") # mm can be added, but has too high execution time (x20)
BLOCK_SIZE_GPU=(1 4 8 16 32 64 128)

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
echo "RunID Filter-type Thread-num Mode Block-size Result" > "$LOG_FILE"
mkdir -p "$PLOTS_PATH" "$PLOTS_PATH/mt" "$PLOTS_PATH/st"

if [[ ! -e "$TEST_FILE" ]]; then
	convert -size 16000x16000 pattern:checkerboard "$TEST_FILE" 2>/dev/null || \
		convert -size 16000x16000 pattern:checkerboard checker.bmp && TEST_FILE="checker.bmp"
fi

echo -e "\nRunning GPU tests (block size = work group size)"
for fil in "${FILTERS[@]}"; do
	for bs in "${BLOCK_SIZE_GPU[@]}"; do
		for i in $(seq 1 "$RUN_NUM"); do
			echo -n "$i " >> "$LOG_FILE"
			"$BIN" -gpu "$TEST_FILE" --filter="$fil" --mode=by_row --block="$bs" --log=1
		done
	done
done

python3 "$SD/avg-plots.py"
python3 "$SD/gpu-plots.py"
