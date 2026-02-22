#!/bin/bash

SD=$(dirname "$(realpath "$0")")
BASEDIR=$(dirname "$SD")
BD="$BASEDIR"
BUILD_DIR="${BUILD_DIR:-$BD/build}"
PLOTS_PATH="$SD/plots/"
FILES_COUNT=20
RUN_NUM=25
QTHREADSMIX=("1,1,1" "2,2,2" "3,3,3" "5,5,5" "1,2,1" "1,1,2" "2,1,1" "1,2,2" "3,2,2" "2,3,3" "1,3,3" "1,4,4" "1,6,6")
MODES=("by_row" "by_column" "by_grid")

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake -B "$BUILD_DIR"
fi
cmake --build "$BUILD_DIR"

BIN="$BUILD_DIR/bmp-conv"
if [[ ! -x "$BIN" ]]; then
    echo "Error: executable not found: $BIN"
    exit 1
fi

clean_plot_dir() {
	mkdir -p "$PLOTS_PATH/q-mode"
	rm -rf "$PLOTS_PATH"/q-mode/*.png
}

INPUT_FILES=()
for ((i=1; i<=FILES_COUNT; i++)); do
	INPUT_FILES+=(image4.bmp)
done

clean_plot_dir
cd "$BD" || exit 1
mkdir -p "$SD/logs"

for mode in "${MODES[@]}"; do
	for mix in "${QTHREADSMIX[@]}"; do
		rm -f "$SD/logs/cpu-queue-timings.dat"
		for i in $(seq 1 "$RUN_NUM"); do
			"$BIN" -queue-mode "${INPUT_FILES[@]}" --mode="$mode" --filter=gg --block=15 --rww="$mix" --log=1
		done
		python3 "$SD/qmt-plots.py" --mix="$mix"
	done
done

python3 "$SD/qmt-plot-summary.py"
