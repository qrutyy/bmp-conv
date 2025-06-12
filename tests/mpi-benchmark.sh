#!/bin/bash

SD=$(dirname "$(realpath "$0")") # script directory
BASEDIR=$(dirname "$SD")  # Parent directory of script's location
BD="$BASEDIR"

LOG_FILE="$SD/timing-results.dat" 
PLOTS_PATH="$SD/plots/"
RUN_NUM=25

TEST_FILE="image7.bmp" # as a small image - image6.bmp was used
# image 7 isn't presented in the repo, it will be generated with the "magick" tool
FILTER="gg"
BLOCK_SIZE=("4" "8" "16" "32" "64" "128")
PROC_NUMS=("2" "3" "4" "5" "6" "7" "8")
MODES=("by_row" "by_column")

make -C "$BD" build-f 

echo "RunID Process-num Filter ThreadNum_logged Mode Block-size Result" > "$LOG_FILE"

echo -e "Generating $TEST_FILE with the size = 1GB..."
magick -size 16384x16384 xc:black $SD/../test-img/image7.bmp

mkdir -p "$PLOTS_PATH/mpi"
echo -e "\nRunning MPI tests"

for proc_num in "${PROC_NUMS[@]}"; do
    for mode in "${MODES[@]}"; do
        for bs in "${BLOCK_SIZE[@]}"; do
            for i in $(seq 1 "$RUN_NUM"); do
                echo -n "$i $proc_num " >> "$LOG_FILE"

                make -C "$BD" run-mpi-mode MPI_NP="$proc_num" \
                     VALGRIND_PREFIX="" INPUT_TF="$TEST_FILE" \
                     FILTER_TYPE="$FILTER" BLOCK_SIZE="$bs" \
                     COMPUTE_MODE="$mode" THREADNUM="1" 
            done
        done
    done
done

echo -e "\nRunning Python plotting script"
python3 "$SD/mpi-plots.py"
