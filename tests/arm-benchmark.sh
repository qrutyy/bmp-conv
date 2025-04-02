#!/bin/bash

# --- Configuration ---
ITERATIONS=40
INPUT_FILE="image4.bmp"
FILTER_TYPE="gg"
THREAD_NUM=4
COMPUTE_MODE="by_grid"
BLOCK_SIZE=32
MAKE_DIR="../" # Path to the Makefile directory

# --- Make Commands ---
MAKE_CMD_ECORES="make -C ${MAKE_DIR} run-mac-e-cores"
MAKE_CMD_PCORES="make -C ${MAKE_DIR} run-mac-p-cores" # Or 'make run'

# --- Parameters for Make commands ---
MAKE_PARAMS="INPUT_TF=${INPUT_FILE} FILTER_TYPE=${FILTER_TYPE} THREAD_NUM=${THREAD_NUM} COMPUTE_MODE=${COMPUTE_MODE} BLOCK_SIZE=${BLOCK_SIZE}"

# --- Arrays to store times ---
e_core_times=()
p_core_times=()

# --- Function to extract time from output ---
extract_time() {
  local output="$1"
  # Extracts the number before " seconds" in the RESULT line
  echo "$output" | grep 'RESULT:.*time = ' | awk '{print $(NF-1)}'
}

# --- Function to calculate and print statistics ---
calculate_stats() {
  local times_array=("$@")
  local n=${#times_array[@]}

  if [ "$n" -eq 0 ]; then
    echo "Error: No data for statistics calculation."
    return 1
  fi

  # Use awk for calculations
  stats=$(printf "%s\n" "${times_array[@]}" | sort -n | awk -v n=$n '
    BEGIN {
      sum = 0; sumsq = 0; min = "";
      p95_idx = int(0.95 * n + 0.999); # 95th percentile index (round up)
    }
    {
      # Store first value as min
      if (NR == 1) min = $1;
      # Store value at the 95th percentile index
      if (NR == p95_idx) p95 = $1;
      # Accumulate sum and sum of squares
      sum += $1;
      sumsq += $1 * $1;
      # Last value after sorting is max
      max = $1;
    }
    END {
      mean = sum / n;
      # Calculate variance: E[X^2] - (E[X])^2
      variance = sumsq / n - mean * mean;
      # Prevent negative variance due to floating point inaccuracies
      if (variance < 0) variance = 0;
      stdev = sqrt(variance);

      # Print statistics
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
  echo "$stats"
}

# --- Running tests on E-cores ---
echo "========================================="
echo " Running tests on E-cores (${ITERATIONS} times)"
echo "========================================="

for (( i=1; i<=ITERATIONS; i++ )); do
  echo -n "Running E-cores $i/$ITERATIONS... "
  # Execute command, redirect stderr to stdout
  output=$(eval "${MAKE_CMD_ECORES} ${MAKE_PARAMS}" 2>&1)
  time_val=$(extract_time "$output")

  # Check if time extraction was successful
  if [[ "$time_val" =~ ^[0-9.]+$ ]]; then
    echo "Time: ${time_val} s"
    e_core_times+=("$time_val")
  else
    echo "Error extracting time! Skipping."
    # echo "Command output: $output" # Uncomment for debugging
  fi
done

# --- Running tests on P-cores ---
echo ""
echo "========================================="
echo " Running tests on P-cores (${ITERATIONS} times)"
echo "========================================="

for (( i=1; i<=ITERATIONS; i++ )); do
  echo -n "Running P-cores $i/$ITERATIONS... "
  # Execute command, redirect stderr to stdout
  output=$(eval "${MAKE_CMD_PCORES} ${MAKE_PARAMS}" 2>&1)
  time_val=$(extract_time "$output")

  # Check if time extraction was successful
  if [[ "$time_val" =~ ^[0-9.]+$ ]]; then
    echo "Time: ${time_val} s"
    p_core_times+=("$time_val")
  else
    echo "Error extracting time! Skipping."
    # echo "Command output: $output" # Uncomment for debugging
  fi
done

# --- Statistics Output ---
echo ""
echo "========================================="
echo " E-cores Statistics"
echo "========================================="
calculate_stats "${e_core_times[@]}" # Pass array elements correctly

echo ""
echo "========================================="
echo " P-cores Statistics"
echo "========================================="
calculate_stats "${p_core_times[@]}" # Pass array elements correctly

echo ""
echo "ARM benchmark is done"

