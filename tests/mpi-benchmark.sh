#!/bin/bash

SD=$(dirname "$(realpath "$0")")
BASEDIR=$(dirname "$SD")
BD="$BASEDIR"
BUILD_DIR="${BUILD_DIR:-$BD/build}"

LOG_FILE="$SD/logs/cpu-timing-results.dat"
PLOTS_PATH="$SD/plots/"
RUN_NUM=25
UNIFIED_HEADER="RunID ProcessNum Backend Mode Filter ThreadNum ComputeMode BlockSize Result"

TEST_FILE="image7.bmp"
FILTER="gg"
BLOCK_SIZE=(4 8 16 32 64 128)
PROC_NUMS=(2 3 4 5 6 7 8)
MODES=("by_row" "by_column")

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake -B "$BUILD_DIR" -DENABLE_MPI=ON
fi
cmake --build "$BUILD_DIR"

BIN="$BUILD_DIR/bmp-conv"
if [[ ! -x "$BIN" ]]; then
    echo "Error: executable not found: $BIN"
    exit 1
fi

mkdir -p "$SD/logs"
echo "$UNIFIED_HEADER" > "$LOG_FILE"

echo "Generating $TEST_FILE (1GB)..."
mkdir -p "$BD/test-img"
magick -size 16384x16384 xc:black "$BD/test-img/$TEST_FILE" 2>/dev/null || \
    convert -size 16384x16384 xc:black "$BD/test-img/$TEST_FILE"

cd "$BD" || exit 1
mkdir -p "$PLOTS_PATH/mpi"
echo -e "\nRunning MPI tests"

for proc_num in "${PROC_NUMS[@]}"; do
    for mode in "${MODES[@]}"; do
        for bs in "${BLOCK_SIZE[@]}"; do
            for i in $(seq 1 "$RUN_NUM"); do
                echo -n "$i $proc_num " >> "$LOG_FILE"
                mpirun -np "$proc_num" "$BIN" -cpu -mpi "test-img/$TEST_FILE" \
                    --filter="$FILTER" --mode="$mode" --block="$bs" --log=1
            done
        done
    done
done

echo -e "\nRunning Python plotting script"
python3 "$SD/mpi-plots.py"
