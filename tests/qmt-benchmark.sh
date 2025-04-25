#!/bin/bash

SD=$(dirname "$(realpath "$0")")
BASEDIR=$(dirname "$SD")  
BD="$BASEDIR"
PLOTS_PATH="$SD/plots/"
FILES_COUNT=20
RUN_NUM=25
QTHREADSMIX=("1,1,1" "2,2,2" "3,3,3" "5,5,5" "1,2,1" "1,1,2" "2,1,1" "1,2,2" "3,2,2" "2,3,3"  "1,3,3" "1,4,4" "1,6,6" )
MODES=("by_row" "by_column" "by_grid") # removed by_pixel due to too big execution time

clean_plot_dir() {
	rm -rf "$PLOTS_PATH"/q-mode/*.png
}

set_input_images() {
	input=""
	for ((i=1; i<=FILES_COUNT; i++)); do
		input+="image4.bmp "
	done
	echo "$input"
}

clean_plot_dir
set_input_images

for mode in "${MODES[@]}"; do
	for mix in "${QTHREADSMIX[@]}"; do 
		rm -rf "$SD/queue-timings.dat"
		for i in $(seq 1 "$RUN_NUM"); do
			make -C "$BD" run-q-mode VALGRIND_PREFIX="" COMPUTE_MODE="$mode" INPUT_TF="$input" FILTER_TYPE=gg BLOCK_SIZE=5 RWW_MIX="$mix"
		done

		python3 "$SD/qmt-plots.py" --mix="$mix"
	done
done

python3 "$SD/qmt-plot-summary.py"
