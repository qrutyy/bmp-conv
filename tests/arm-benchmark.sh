#!/bin/bash

ITERATIONS=40
INPUT_FILE="image4.bmp"
FILTER_TYPE="gg"
THREAD_NUM=4
COMPUTE_MODE="by_grid"
BLOCK_SIZE=32
MAKE_DIR="../"

MAKE_CMD_ECORES="make -C ${MAKE_DIR} run-mac-e-cores"
MAKE_CMD_PCORES="make -C ${MAKE_DIR} run-mac-p-cores" # Or 'make run'

MAKE_PARAMS="INPUT_TF=${INPUT_FILE} FILTER_TYPE=${FILTER_TYPE} THREAD_NUM=${THREAD_NUM} COMPUTE_MODE=${COMPUTE_MODE} BLOCK_SIZE=${BLOCK_SIZE}"

e_core_times=()
p_core_times=()

extract_time() {
  local output="$1"
  echo -e "$output" | grep 'RESULT:.*time = ' | awk '{print $(NF-1)}'
}

calculate_stats() {
  local times_array=("$@")
  local n=${#times_array[@]}

  if [ "$n" -eq 0 ]; then
    echo -e "Error: No data for statistics calculation."
    return 1
  fi

  stats=$(printf "%s\n" "${times_array[@]}" | sort -n | awk -v n="$n" '
    BEGIN {
      sum = 0; sumsq = 0; min = "";
      p95_idx = int(0.95 * n + 0.999); # 95th percentile index (round up)
    }
    {
      if (NR == 1) min = $1;
      if (NR == p95_idx) p95 = $1;
      sum += $1;
      sumsq += $1 * $1;
      max = $1;
    }
    END {
      mean = sum / n;
      # Calculate variance: E[X^2] - (E[X])^2
      variance = sumsq / n - mean * mean;
      if (variance < 0) variance = 0;
      stdev = sqrt(variance);

      printf "Count:             %d\n", n;
      printf "Min:               %.6f s\n", min;
      printf "Max:               %.6f s\n", max;
      printf "Mean:              %.6f s\n", mean;
      printf "StdDev:            %.6f s\n", stdev;
      # Ensure percentile was found (if n is large enough)
      if (p95 != "") {
          printf "95th Percentile:   %.6f s\n", p95;
      } else {
          printf "95th Percentile:   N/A (n=%d too small)\n", n;
      }
    }'
  )
  echo -e "$stats"
}

echo -e "\nRunning tests on E-cores (${ITERATIONS} times)"

for (( i=1; i<=ITERATIONS; i++ )); do
  echo -e "Running E-cores $i/$ITERATIONS... "
  output=$(eval "${MAKE_CMD_ECORES} ${MAKE_PARAMS}" 2>&1)
  time_val=$(extract_time "$output")

  if [[ "$time_val" =~ ^[0-9.]+$ ]]; then
    echo -e "Time: ${time_val} s"
    e_core_times+=("$time_val")
  else
    echo -e "Error extracting time! Skipping."
  fi
done

echo -e "\nRunning tests on P-cores (${ITERATIONS} times)"

for (( i=1; i<=ITERATIONS; i++ )); do
  echo -e "Running P-cores $i/$ITERATIONS... "
  output=$(eval "${MAKE_CMD_PCORES} ${MAKE_PARAMS}" 2>&1)
  time_val=$(extract_time "$output")

  if [[ "$time_val" =~ ^[0-9.]+$ ]]; then
    echo -e "Time: ${time_val} s\n"
    p_core_times+=("$time_val")
  else
    echo -e "Error extracting time! Skipping.\n"
  fi
done

echo -e "\nE-cores Statistics\n"
calculate_stats "${e_core_times[@]}" 

echo -e "\nP-cores Statistics\n"
calculate_stats "${p_core_times[@]}" 

echo -e "\nARM benchmark is done\n"

