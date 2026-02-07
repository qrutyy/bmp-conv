#!/bin/bash

SD=$(dirname "$(realpath "$0")")
BASEDIR=$(dirname "$SD")
BD="$BASEDIR"

IMG_FOLDER="$BD/test-img/"

TP_NUM=(2 3 8) 
MODES=("by_row" "by_column" "by_pixel" "by_grid")
MPI_MODES=("by_row" "by_column")
FILTERS=("co" "gg" "bo") # "sh" "bb" "gb" "em" "mb" "mg" 
TEST_FILE="image5.bmp"
BLOCK_SIZE=("4" "128") # u can extend with  "8" "16" "32" "64" 
VG_PREFIX=""
QMT_INPUT_FILES=("image1.bmp" "image2.bmp" "image3.bmp" "image4.bmp")
RWW_COMBINATIONS=("1,1,1" "1,3,1" "2,3,2")

# shorten the args and set up specific build cfg
if [[ "$1" == "ci" || "$1" == "ci-memcheck" || "$1" == "ci-helgrind" ]]; then 
	TP_NUM=( 3 ) 
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

    if [[ "$mode" == "st" ]]; then
        if diff -q "${IMG_FOLDER}seq_out_${filename}" "${IMG_FOLDER}pix.bmp" > /dev/null; then
            echo -e "Files are identical\n"
        else
			echo -e "Files differ (ST (by_row, by_column, by_grid) vs ST pix)!\n"
            diff --color=always "${IMG_FOLDER}seq_out_${filename}" "${IMG_FOLDER}pix.bmp"
            exit 1
        fi
    elif [[ "$mode" == "mt" ]]; then
        if diff -q "${IMG_FOLDER}seq_out_${filename}" "${IMG_FOLDER}rcon_out_${filename}" > /dev/null; then
            echo -e "Files are identical\n"
        else
            echo -e "Files differ (MT vs SEQ)!\n"
            diff --color=always "${IMG_FOLDER}seq_out_${filename}" "${IMG_FOLDER}rcon_out_${filename}"
            exit 1
        fi
	elif [[ "$mode" == "qmt" ]]; then
        if diff -q "${IMG_FOLDER}rcon_out_${filename}" "${IMG_FOLDER}qmt_out_${filename}" > /dev/null; then
            echo -e "Files are identical\n"
        else
			echo "Files differ (QMT vs MT)!"
			diff --color=always "${IMG_FOLDER}rcon_out_${filename}" "${IMG_FOLDER}qmt_out_${filename}"
            exit 1
        fi
    elif [[ "$mode" == "mpi" ]]; then
         if diff -q "${IMG_FOLDER}rcon_out_${filename}" "${IMG_FOLDER}mpi_out_${filename}" > /dev/null; then
            echo -e "Files are identical\n"
         else
            echo -e "Files differ (MPI vs MT)!\n"
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
	echo "$VG_PREFIX"
	make -C "$BD" run VALGRIND_PREFIX="$VG_PREFIX" INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=1 BLOCK_SIZE=1 LOG=0 OUTPUT_FILE="pix.bmp"

	for bs in "${BLOCK_SIZE[@]}"; do
		make -C "$BD" run VALGRIND_PREFIX="$VG_PREFIX" INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=1 BLOCK_SIZE="$bs" LOG=0 > /dev/null 
		compare_results "$TEST_FILE" "st"
    done 
done


echo -e "\nRunning multithreaded verification tests"
for mode in "${MODES[@]}"; do
	for fil in "${FILTERS[@]}"; do
		for bs in "${BLOCK_SIZE[@]}"; do 
			make -C "$BD" run VALGRIND_PREFIX="$VG_PREFIX" INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=1 BLOCK_SIZE="$bs" COMPUTE_MODE="$mode" LOG=0 

			for th in "${TP_NUM[@]}"; do
				make -C "$BD" run VALGRIND_PREFIX="$VG_PREFIX" INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM="$th" BLOCK_SIZE="$bs" COMPUTE_MODE="$mode" LOG=0 > /dev/null 
				compare_results "$TEST_FILE" "mt"
            done   
		done
	done
done

echo -e "\nRunning queue-mode verification tests"
for mode in "${MODES[@]}"; do
	for fil in "${FILTERS[@]}"; do
		for bs in "${BLOCK_SIZE[@]}"; do

			for file in "${QMT_INPUT_FILES[@]}"; do
				make -C "$BD" run VALGRIND_PREFIX="" INPUT_TF="$file" FILTER_TYPE="$fil" THREAD_NUM=4 LOG=0 
			done

			for rww in "${RWW_COMBINATIONS[@]}"; do
                echo "QMT Test: mode=$mode filter=$fil block_size=$bs rww=$rww files=(${QMT_INPUT_FILES[*]})"

                # Construct the input file list for the make command
                input_file_args=$(printf "%s " "${QMT_INPUT_FILES[@]}") # Space-separated list
				echo "$input_file_args"

                make -C "$BD" run-q-mode \
                    VALGRIND_PREFIX="$VG_PREFIX" \
                    INPUT_TF="$input_file_args" \
                    FILTER_TYPE="$fil" \
                    COMPUTE_MODE="$mode" \
                    BLOCK_SIZE="$bs" \
                    RWW_MIX="$rww" \
                    LOG=0 

                echo "QMT Verify: mode=$mode filter=$fil block_size=$bs rww=$rww"
                for infile in "${QMT_INPUT_FILES[@]}"; do
                    compare_results "$infile" "qmt" 
                done
			done 
		done 
	done 
done 

echo -e "\nRunning mpi-mode verification tests"
for mode in "${MPI_MODES[@]}"; do
	for fil in "${FILTERS[@]}"; do
		make -C "$BD" run VALGRIND_PREFIX="" INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" THREAD_NUM=4 LOG=0 BLOCK_SIZE=10

		for pc in "${TP_NUM[@]}"; do
			make -C "$BD" run-mpi-mode VALGRIND_PREFIX="$VG_PREFIX" INPUT_TF="$TEST_FILE" FILTER_TYPE="$fil" MPI_NP="$pc" COMPUTE_MODE="$mode" LOG=0 

			echo -e "Comparing mpi with $pc processes and $fil filter\n"
			compare_results "$TEST_FILE" "mpi"
		done
	done
done
