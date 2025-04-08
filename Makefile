SHELL=/bin/sh

# === Compiler and Flags ===
CC = gcc
CFLAGS = -Wall -Wpedantic -Wextra -g -std=c99
CPPFLAGS = -D_POSIX_C_SOURCE=200809L # use the specific posix standart that includes barriers
LDLIBS = -lm 

# === Project Structure ===
TARGET_EXEC := bmp-conv

SRC_DIRS := src src/utils src/st-mode src/mt-mode src/qmt-mode logger libbmp
VPATH := $(SRC_DIRS)

SRCS := bmp-conv.c args-parse.c filters.c threads-general.c utils.c \
         st-mode/exec.c \
         mt-mode/compute.c mt-mode/exec.c \
         qmt-mode/exec.c qmt-mode/queue.c qmt-mode/threads.c \
         log.c \
         libbmp.c

BUILD_DIR := obj
OBJS := $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))

RM := rm -f

# === Runtime Parameters (with defaults, can be overridden) ===
INPUT_TF    ?= image2.bmp
FILTER_TYPE ?= mb
THREAD_NUM  ?= 1
COMPUTE_MODE?= by_column
BLOCK_SIZE  ?= 5
OUTPUT_FILE ?= "" # Default to empty, let the program handle it
LOG         ?= 1
RWW_MIX     ?= 1,1,1

# === Build Targets ===
.DEFAULT_GOAL := all
all: $(TARGET_EXEC)

# Link the executable from object files
$(TARGET_EXEC): $(OBJS)
	@echo "\nLinking $@..."
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)
	@echo "Build complete: $(TARGET_EXEC)"

# Pattern rule to compile .c files into .o files in the BUILD_DIR
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR) # keep the order-only prerequisite for the top dir
	@echo "\nCompiling $< -> $@"
	@mkdir -p $(dir $@) # creating obj subdir 
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $@

# === Run Targets ===

# Base run command arguments
RUN_ARGS := $(INPUT_TF) --filter=$(FILTER_TYPE) --threadnum=$(THREAD_NUM) --mode=$(COMPUTE_MODE) --block=$(BLOCK_SIZE) --output=$(OUTPUT_FILE) --log=$(LOG)
# Queue mode specific arguments
RUN_Q_ARGS := -queue-mode $(INPUT_TF) --mode=$(COMPUTE_MODE) --filter=$(FILTER_TYPE) --block=$(BLOCK_SIZE) --rww=$(RWW_MIX)

# Run the program with standard arguments
run: all
	@echo "\nRunning: ./$(TARGET_EXEC) $(RUN_ARGS)"
	./$(TARGET_EXEC) $(RUN_ARGS)

# Run specifically on macOS E-cores
run-mac-e-cores: all
	@echo "\nRunning on E-cores: taskpolicy -c background ./$(TARGET_EXEC) $(RUN_ARGS)"
	taskpolicy -c background ./$(TARGET_EXEC) $(RUN_ARGS)

# Run on macOS P-cores (using default policy which often prioritizes P-cores or uses all)
run-mac-p-cores: all
	@echo "\nRunning on P-cores (default policy): ./$(TARGET_EXEC) $(RUN_ARGS)"
	./$(TARGET_EXEC) $(RUN_ARGS)

run-q-mode: all
	@echo "\nRunning Queue Mode: ./$(TARGET_EXEC) $(RUN_Q_ARGS)"
	./$(TARGET_EXEC) $(RUN_Q_ARGS)

clean:
	@echo "\nCleaning build artifacts..."
	$(RM) $(TARGET_EXEC)
	$(RM) -r $(BUILD_DIR)
	$(RM) tests/*.dat src/*.out 

.PHONY: all clean run run-mac-e-cores run-mac-p-cores run-q-mode

.SUFFIXES:

