#!/bin/bash
set -euo pipefail

SD=$(dirname "$(realpath "$0")")
BASEDIR=$(dirname "$SD")
BD="$BASEDIR"

IMG_FOLDER="$BD/test-img/"

TP_NUM=(2 3 8)
MODES=("by_row" "by_column" "by_pixel" "by_grid")
MPI_MODES=("by_row" "by_column")
FILTERS=("co" "gg" "bo")
TEST_FILE="image5.bmp"
BLOCK_SIZE=("4" "128")
VG_PREFIX=""
QMT_INPUT_FILES=("image1.bmp" "image2.bmp" "image3.bmp" "image4.bmp")
RWW_COMBINATIONS=("1,1,1" "1,3,1" "2,3,2")

# === CI shortcuts ===
if [[ "${1:-}" == "ci" || "${1:-}" == "ci-memcheck" || "${1:-}" == "ci-helgrind" ]]; then
    TP_NUM=(3)
    MODES=("by_row")
    FILTERS=("gg")
    BLOCK_SIZE=("32")
    QMT_INPUT_FILES=("image1.bmp" "image2.bmp" "image3.bmp")
    if [[ "$1" == "ci-memcheck" ]]; then
        VG_PREFIX="valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1"
    fi
    if [[ "$1" == "ci-helgrind" ]]; then
        VG_PREFIX="valgrind --tool=helgrind --error-exitcode=1"
    fi
fi

compare_results() {
    local filename=$1
    local mode=$2
    local diff_file ref_file

    case "$mode" in
        st)   diff_file="${IMG_FOLDER}seq_out_${filename}"; ref_file="${IMG_FOLDER}pix.bmp";;
        mt)   diff_file="${IMG_FOLDER}seq_out_${filename}"; ref_file="${IMG_FOLDER}rcon_out_${filename}";;
        qmt)  diff_file="${IMG_FOLDER}rcon_out_${filename}"; ref_file="${IMG_FOLDER}qmt_out_${filename}";;
        mpi)  diff_file="${IMG_FOLDER}rcon_out_${filename}"; ref_file="${IMG_FOLDER}mpi_out_${filename}";;
        *)    diff_file="${IMG_FOLDER}seq_out_${filename}"; ref_file="${IMG_FOLDER}rcon_out_${filename}";;
    esac

    if diff -q "$diff_file" "$ref_file" >/dev/null; then
        echo "✅ Files identical: $filename ($mode)"
    else
        echo "❌ Files differ: $filename ($mode)"
        diff --color=always "$diff_file" "$ref_file"
        exit 1
    fi
}

# === Helper to configure and build a target ===
run_target() {
    local target=$1
    shift
    # Configure CMake with specific arguments
    cmake -S "$BD" -B "$BD/build" "$@" 
    # Build the target
    cmake --build "$BD/build" --target "$target"
}

# === ST tests ===
echo -e "\n=== Single-threaded verification tests ==="
for fil in "${FILTERS[@]}"; do
    echo "Filter: $fil"
    run_target run \
        -DINPUT_TF="$TEST_FILE" \
        -DFILTER_TYPE="$fil" \
        -DTHREAD_NUM=1 \
        -DBLOCK_SIZE=1 \
        -DLOG=0 \
        -DOUTPUT_FILE="pix.bmp" \
        -DVALGRIND_PREFIX="$VG_PREFIX"

    for bs in "${BLOCK_SIZE[@]}"; do
        run_target run \
            -DINPUT_TF="$TEST_FILE" \
            -DFILTER_TYPE="$fil" \
            -DTHREAD_NUM=1 \
            -DBLOCK_SIZE="$bs" \
            -DLOG=0
        compare_results "$TEST_FILE" "st"
    done
done

# === MT tests ===
echo -e "\n=== Multi-threaded verification tests ==="
for mode in "${MODES[@]}"; do
    for fil in "${FILTERS[@]}"; do
        for bs in "${BLOCK_SIZE[@]}"; do
            run_target run \
                -DINPUT_TF="$TEST_FILE" \
                -DFILTER_TYPE="$fil" \
                -DTHREAD_NUM=1 \
                -DBLOCK_SIZE="$bs" \
                -DCOMPUTE_MODE="$mode" \
                -DLOG=0

            for th in "${TP_NUM[@]}"; do
                run_target run \
                    -DINPUT_TF="$TEST_FILE" \
                    -DFILTER_TYPE="$fil" \
                    -DTHREAD_NUM="$th" \
                    -DBLOCK_SIZE="$bs" \
                    -DCOMPUTE_MODE="$mode" \
                    -DLOG=0
                compare_results "$TEST_FILE" "mt"
            done
        done
    done
done

# === QMT tests ===
echo -e "\n=== Queue-mode verification tests ==="
for mode in "${MODES[@]}"; do
    for fil in "${FILTERS[@]}"; do
        for bs in "${BLOCK_SIZE[@]}"; do
            # Run all input files first
            for file in "${QMT_INPUT_FILES[@]}"; do
                run_target run \
                    -DINPUT_TF="$file" \
                    -DFILTER_TYPE="$fil" \
                    -DTHREAD_NUM=4 \
                    -DLOG=0
            done

            # Run queue mode for all RWW combinations
            for rww in "${RWW_COMBINATIONS[@]}"; do
                input_files=$(IFS=" "; echo "${QMT_INPUT_FILES[*]}")
                echo "QMT Test: mode=$mode filter=$fil block_size=$bs rww=$rww files=(${input_files})"

                run_target run-q-mode \
                    -DINPUT_TF="$input_files" \
                    -DFILTER_TYPE="$fil" \
                    -DCOMPUTE_MODE="$mode" \
                    -DBLOCK_SIZE="$bs" \
                    -DRWW_MIX="$rww" \
                    -DLOG=0

                for infile in "${QMT_INPUT_FILES[@]}"; do
                    compare_results "$infile" "qmt"
                done
            done
        done
    done
done

# === MPI tests ===
echo -e "\n=== MPI-mode verification tests ==="
for mode in "${MPI_MODES[@]}"; do
    for fil in "${FILTERS[@]}"; do
        # Baseline MT run
        run_target run \
            -DINPUT_TF="$TEST_FILE" \
            -DFILTER_TYPE="$fil" \
            -DTHREAD_NUM=4 \
            -DBLOCK_SIZE=10 \
            -DLOG=0

        for pc in "${TP_NUM[@]}"; do
            run_target run-mpi-mode \
                -DINPUT_TF="$TEST_FILE" \
                -DFILTER_TYPE="$fil" \
                -DMPI_NP="$pc" \
                -DCOMPUTE_MODE="$mode" \
                -DLOG=0

            echo "Comparing MPI output with $pc processes and filter $fil"
            compare_results "$TEST_FILE" "mpi"
        done
    done
done

echo -e "\n✅ All tests completed successfully."
