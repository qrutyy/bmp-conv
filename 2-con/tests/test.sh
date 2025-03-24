#!/bin/bash

RUN_NUM=15
TEST_FILE="image4.bmp"
FILTERS=("mb" "bb" "gb" "co" "sh" "em" "mm" "gg")
BLOCK_SIZE=("4" "8" "16" "32" "64" "128")
THREADNUM=4
LOG_FILE="timing-results.dat"
PLOTS_PATH="./plots/"
MODES=("by_row" "by_column" "by_grid") # removed by_pixel due to too big execution time
IMG_FOLDER=../test-img/

FILTER_PAIRS="gb,sh sh,gb mb,sh sh,mb gg,sh sh,gg"

IFS=' ' read -r -a pairs <<< "$FILTER_PAIRS"

for pair in "${pairs[@]}"; do
    IFS=',' read -r f1 f2 <<< "$pair"
    echo "f1: $f1, f2: $f2"
done


compare_results() {
	filename=$1
	if diff -q ${IMG_FOLDER}seq_out_${filename} ${IMG_FOLDER}rcon_out_${filename} > /dev/null; then
		echo -e "Files are identical\n"
	else
		echo -e "Files differ\n"
	    diff seq_out_${filename} rcon_out_${filename}
		exit -1
	fi
}

rm -rf ../mt-con ../../1-seq/seq-con
make -C .. clean
make -C .. mt-con
make -C ../../1-seq/ seq-con

echo "RunID Filter-type Thread-num Mode Block-size Result" > $LOG_FILE 

mkdir -p $PLOTS_PATH
cd ..

echo -e "\nRunning mt tests with 1 filter"
for mode in "${MODES[@]}"; do
	for fil in "${FILTERS[@]}"; do
		for bs in "${BLOCK_SIZE[@]}"; do
			./../1-seq/seq-con $TEST_FILE $fil

			for i in $(seq 1 "$RUN_NUM"); do
				echo -n "$i " >> tests/$LOG_FILE
				./mt-con $TEST_FILE $fil --threadnum=$THREADNUM --mode=$mode --block=$bs 
				compare_results $TEST_FILE
			done
		done
	done
done

python3 tests/avg_plots.py

for pair in "${pairs[@]}"; do
	IFS=',' read -r f1 f2 <<< "$pair"
	echo -e "SEQUENTIAL MODE:\n"

	./../1-seq/seq-con $TEST_FILE $f1 --output=${f1}_${TEST_FILE}
	./../1-seq/seq-con ${f1}_${TEST_FILE} $f2 --output=seq_out_${f1}_${f2}_${TEST_FILE}

	for i in $(seq 1 "$RUN_NUM"); do 
		echo -n "$i " >> tests/$LOG_FILE
		echo -e "MULTITHREADED MODE:\n"
			
		./mt-con $TEST_FILE $f1 --threadnum=$THREADNUM --mode=by_grid --block=16 --output=${f1}_${TEST_FILE}
		echo -n "$i " >> tests/$LOG_FILE
		./mt-con ${f1}_${TEST_FILE} $f2 --threadnum=$THREADNUM --mode=by_grid --block=16 --output=rcon_out_${f1}_${f2}_${TEST_FILE}

		compare_results ${f1}_${f2}_${TEST_FILE}
	done
done

python3 tests/comp-plots.py

