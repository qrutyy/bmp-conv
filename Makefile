SHELL=/bin/sh

# === Compiler and Flags ===
CC = gcc
MPICC = mpicc
CFLAGS = -Wall -Wpedantic -Wextra -std=c99 -DLOG_USE_COLOR -DNDEBUG -O3

MPICFLAGS = $(CFLAGS) -DUSE_MPI -DLOG_USE_COLOR
CPPFLAGS = -D_POSIX_C_SOURCE=200809L # use the specific posix standart that includes barriers
LDLIBS = -lm 

# === Project Structure ===
TARGET_BASE_NAME := bmp-conv
TARGET_NO_MPI := $(TARGET_BASE_NAME)
TARGET_MPI := $(TARGET_BASE_NAME)-mpi

SRC_DIRS := src src/utils src/st-mode src/mt-mode src/qmt-mode src/mpi-mode logger libbmp
VPATH := $(SRC_DIRS)

SRCS_NO_MPI := bmp-conv.c utils/args-parse.c utils/filters.c utils/threads-general.c utils/utils.c \
         st-mode/exec.c \
         mt-mode/compute.c mt-mode/exec.c \
         qmt-mode/exec.c qmt-mode/queue.c qmt-mode/threads.c \
         log.c \
         libbmp.c
SRCS_MPI := $(SRCS_NO_MPI) mpi-mode/exec.c mpi-mode/utils.c mpi-mode/phases.c mpi-mode/rank0-proc.c mpi-mode/filter-comp.c mpi-mode/data-transfer.c

BUILD_DIR_NO_MPI := build/obj_no_mpi
BUILD_DIR_MPI := build/obj_mpi

OBJS_NO_MPI := $(addprefix $(BUILD_DIR_NO_MPI)/, $(SRCS_NO_MPI:.c=.o))
OBJS_MPI := $(addprefix $(BUILD_DIR_MPI)/, $(SRCS_MPI:.c=.o))

RM := rm -f

# === Runtime Parameters (with defaults, can be overridden) ===
INPUT_TF ?= image2.bmp
FILTER_TYPE ?= mb
THREAD_NUM ?= 2
COMPUTE_MODE ?= by_row
BLOCK_SIZE ?= 5
OUTPUT_FILE ?= "" # Default to empty, let the program handle it
LOG ?= 1
RWW_MIX ?= 1,1,1
MPI_NP ?= 2
VALGRIND_PREFIX ?= "" 

# === Build Targets ===
.DEFAULT_GOAL := all
all: $(TARGET_NO_MPI) $(TARGET_MPI)

# --- Non-MPI Build ---
$(TARGET_NO_MPI): $(OBJS_NO_MPI)
	@echo "\nLinking Non-MPI $@..."
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)
	@echo -e "\n=== BUILD COMPLETE: $(TARGET_NO_MPI) ===\n"

$(BUILD_DIR_NO_MPI)/%.o: %.c | $(BUILD_DIR_NO_MPI)
	@echo "\nCompiling Non-MPI $< -> $@"
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR_NO_MPI):
	@mkdir -p $@

# --- MPI Build ---
$(TARGET_MPI): $(OBJS_MPI)
	@echo "\nLinking MPI $@..."
	$(MPICC) $(LDFLAGS) $^ -o $@ $(LDLIBS) 
	@echo -e "\n=== BUILD COMPLETE: $(TARGET_MPI) ===\n"

$(BUILD_DIR_MPI)/%.o: %.c | $(BUILD_DIR_MPI)
	@echo "\nCompiling MPI $< -> $@"
	@mkdir -p $(dir $@)
	$(MPICC) $(CPPFLAGS) $(MPICFLAGS) -c $< -o $@ 

$(BUILD_DIR_MPI):
	@mkdir -p $@

# === Run Targets ===

# Base run command arguments
RUN_ARGS := $(INPUT_TF) --filter=$(FILTER_TYPE) --threadnum=$(THREAD_NUM) --mode=$(COMPUTE_MODE) --block=$(BLOCK_SIZE) --output=$(OUTPUT_FILE) --log=$(LOG)
# Queue mode specific arguments
RUN_Q_ARGS := -queue-mode $(INPUT_TF) --mode=$(COMPUTE_MODE) --filter=$(FILTER_TYPE) --block=$(BLOCK_SIZE) --rww=$(RWW_MIX)
# Arguments  for MPI mode (same as basic, except mpi-mode)
MPI_RUN_ARGS := -mpi-mode $(RUN_ARGS)
# Run the program with standard arguments
run: $(TARGET_NO_MPI)
	@echo "\nRunning: ./$(TARGET_NO_MPI) $(RUN_ARGS)"
	$(VALGRIND_PREFIX) ./$(TARGET_NO_MPI) $(RUN_ARGS)

# Run specifically on macOS E-cores
run-mac-e-cores: $(TARGET_NO_MPI)
	@echo "\nRunning on E-cores: taskpolicy -c background ./$(TARGET_NO_MPI) $(RUN_ARGS)"
	taskpolicy -c background ./$(TARGET_NO_MPI) $(RUN_ARGS)

# Run on macOS P-cores (using default policy which often prioritizes P-cores or uses all)
run-mac-p-cores: $(TARGET_NO_MPI)
	@echo "\nRunning on P-cores (default policy): ./$(TARGET_NO_MPI) $(RUN_ARGS)"
	./$(TARGET_NO_MPI) $(RUN_ARGS)

run-q-mode: $(TARGET_NO_MPI)
	@echo "\nRunning Queue Mode: ./$(TARGET_NO_MPI) $(RUN_Q_ARGS)"
	$(VALGRIND_PREFIX) ./$(TARGET_NO_MPI) $(RUN_Q_ARGS)

# Run the MPI target using mpirun
run-mpi-mode: $(TARGET_MPI)
	@echo "\nRunning MPI mode (NP=$(MPI_NP)): mpirun -np $(MPI_NP) ./$(TARGET_MPI) $(MPI_RUN_ARGS)"
	$(VALGRIND_PREFIX) mpirun -np $(MPI_NP) ./$(TARGET_MPI) $(MPI_RUN_ARGS)

build-f: 
	rm -rf build/
	make

clean:
	@echo "\nCleaning build artifacts..."
	$(RM) $(TARGET_NO_MPI)
	$(RM) -r $(BUILD_DIR_MPI)
	$(RM) -r $(BUILD_DIR_NO_MPI)
	$(RM) tests/*.dat src/*.out 

.PHONY: all clean run run-mac-e-cores run-mac-p-cores run-q-mode

.SUFFIXES:

