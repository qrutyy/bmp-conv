#!/bin/bash

SD=$(dirname "$(realpath "$0")")
BASEDIR=$(dirname "$SD")  
BD="$BASEDIR"
IMG_FOLDER="$BD/test-img/"
LOG_FILE="$SD/timing-results.dat"
PLOTS_PATH="$SD/plots/"
FILES_COUNT=20
QTHREADSMIX=("1,1,1" "1,2,1" "1,1,2" "2,1,1" "1, 2, 2" "2,2,2" "3,3,3" "3,2,2" "2,3,3" "1,3,3" "5,5,5" "1,3,3" "1,4,4" "1,6,6" "1,8,8" "1,10,10")

clean_plot_dir() {
	rm -rf $PLOTS_PATH/q-mode/*.png
}

set_input_images() {
	input=""
	for ((i=1; i<=FILES_COUNT; i++)); do
		input+="image4.bmp "
	done
	echo $input
}

clean_plot_dir
set_input_images

for mix in "${QTHREADSMIX[@]}"; do 
	rm -rf "$SD/queue-timings.dat"

	make -C "$BD" run-q-mode COMPUTE_MODE=by_grid INPUT_TF="$input" FILTER_TYPE=gg BLOCK_SIZE=5 RWW_MIX=$mix
	python3 "$SD/qm-plots.py" --mix=$mix
done
