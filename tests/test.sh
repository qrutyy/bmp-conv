#!/bin/bash

SD=$(dirname "$(realpath "$0")")
BASEDIR=$(dirname "$SD")
BD="$BASEDIR"

IMG_FOLDER="$BD/test-img/"

TP_NUM=(2 3 5 8) 
MODES=("by_row" "by_column" "by_pixel" "by_grid")
MPI_MODES=("by_row" "by_column")
FILTERS=( "gg" "bb" "gb" "em" "mb" "mg" "gg" "bo")
TEST_FILE="image5.bmp"
BLOCK_SIZE=("4" "8" "16" "32" "64" "128")

make -C "$BD" clean
make -C "$BD"

compare_results() {
    local filename=$1
    local mode=$2

    if [[ "$mode" == "st" ]]; then
        if diff -q "${IMG_FOLDER}seq_out_${filename}" "${IMG_FOLDER}pix.bmp" > /dev/null; then
            echo -e "Files are identical\n"
        else
            echo -e "Files differ (ST vs pix)!\n"
            diff --color=always "${IMG_FOLDER}seq_out_${filename}" "${IMG_FOLDER}pix.bmp"
            exit 1
        fi
    elif [[ "$mode" == "mt" ]]; then
        if diff -q "${IMG_FOLDER}seq_out_${filename}" "${IMG_FOLDER}rcon_out_${filename}" > /dev/null; then
            echo -e "Files are identical\n"
        else
            echo -e "Files differ (MT vs seq_out)!\n"
            diff --color=always "${IMG_FOLDER}seq_out_${filename}" "${IMG_FOLDER}rcon_out_${filename}"
            exit 1
        fi
    elif [[ "$mode" == "qmt" ]]; then # Corrected string comparison
        echo -e "TODO QUEUE MODE VERIFICATION\n"
    elif [[ "$mode" == "mpi" ]]; then
         if diff -q "${IMG_FOLDER}rcon_out_${filename}" "${IMG_FOLDER}mpi_out_${filename}" > /dev/null; then
            echo -e "Files are identical\n"
         else
            echo -e "Files differ (MPI vs seq_out)!\n"
            diff --color=always "${IMG_FOLDER}rcon_out_${filename}" "${IMG_FOLDER}mpi_out_${filename}"
            exit 1
         fi
    else
       echo -e "Files differ (Fallback Check)!\n"
       diff --color=always "${IMG_FOLDER}seq_out_${filename}" "${IMG_FOLDER}rcon_out_${filename}"
       exit 1
    fi
}

echo -e "\nRunning single-threaded verification tests"
for fil in "${FILTERS[@]}"; do
	make -C "$BD" run INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=1 BLOCK_SIZE=1 LOG=0 OUTPUT_FILE="pix.bmp" > /dev/null

	for bs in "${BLOCK_SIZE[@]}"; do
		make -C "$BD" run INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=1 BLOCK_SIZE="$bs" LOG=0 > /dev/null 
		compare_results "$TEST_FILE" "st"
    done 
done


echo -e "\nRunning multithreaded verification tests"
for mode in "${MODES[@]}"; do
	for fil in "${FILTERS[@]}"; do
		for bs in "${BLOCK_SIZE[@]}"; do
			make -C "$BD" run INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=1 LOG=0 >/dev/null

			for th in "${TP_NUM[@]}"; do
				make -C "$BD" run INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM="$th" BLOCK_SIZE="$bs" COMPUTE_MODE="$mode" LOG=0 > /dev/null 
				compare_results "$TEST_FILE" "mt"
            done 		
		done
	done
done

echo -e "\nRunning queue-mode verification tests"
# TODO

echo -e "\nRunning mpi-mode verification tests"
for mode in "${MPI_MODES[@]}"; do
	for fil in "${FILTERS[@]}"; do
		make -C "$BD" run INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=2 LOG=0 BLOCK_SIZE=10 > /dev/null

		for pc in "${TP_NUM[@]}"; do
			make -C "$BD" run-mpi-mode INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" MPI_NP="$pc" COMPUTE_MODE="$mode" LOG=0 

			echo -e "Comparing mpi with $pc processes and $fil filter\n"
			compare_results "$TEST_FILE" "mpi"
		done
	done
done
