cmake_minimum_required(VERSION 3.10)
project(bmp-conv C)

# === C standard & compiler flags ===
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wpedantic -Wextra -O3)
add_compile_definitions(
    LOG_USE_COLOR
    NDEBUG
    _POSIX_C_SOURCE=200809L
)

include_directories(. src)

option(ENABLE_MPI "Enable MPI backend" ON)
option(ENABLE_OPENCL "Enable OpenCL backend" ON)

find_package(MPI COMPONENTS C QUIET)
find_package(OpenCL QUIET)

# === Sources ===
set(SRCS_COMMON
	src/bmp-conv.c
	src/utils/args-parse.c
	src/utils/filters.c
	src/utils/threads-general.c
	src/utils/utils.c
	src/backend/compute-backend.c
	src/backend/cpu/cpu-backend.c
	src/backend/cpu/st/st-exec.c
	src/backend/cpu/mt/mt-compute.c
	src/backend/cpu/mt/mt-exec.c
	src/backend/cpu/qmt/qmt-exec.c
	src/backend/cpu/qmt/utils/qmt-queue.c
	src/backend/cpu/qmt/qmt-threads.c
	src/backend/gpu/gpu-backend.c
	src/backend/gpu/core/mw-exec.c
	src/backend/gpu/utils/utils.c
	logger/log.c
	libbmp/libbmp.c
)

set(SRCS_MPI_ONLY
	src/backend/cpu/mpi/mpi-exec.c
	src/backend/cpu/mpi/utils/utils.c
	src/backend/cpu/mpi/comm/phases.c
	src/backend/cpu/mpi/comm/rank0-proc.c
	src/backend/cpu/mpi/comm/data-transfer.c
	src/backend/cpu/mpi/compute/filter-comp.c
)

set(SRCS_OPENCL_ONLY
	src/backend/gpu/gpu-backend.c
	src/backend/gpu/core/mw-exec.c
	src/backend/gpu/utils/utils.c
)

set(SRCS
	${SRCS_COMMON}
)
add_executable(bmp-conv ${SRCS_COMMON})

if(ENABLE_MPI AND MPI_C_FOUND)
    target_sources(bmp-conv PRIVATE ${SRCS_MPI_ONLY})
    target_compile_definitions(bmp-conv PRIVATE USE_MPI)
    target_link_libraries(bmp-conv PRIVATE m MPI::MPI_C)
endif()

if(ENABLE_OPENCL AND OpenCL_FOUND)
    target_sources(bmp-conv PRIVATE ${SRCS_OPENCL_ONLY})
    target_compile_definitions(bmp-conv PRIVATE USE_OPENCL)
    target_link_libraries(bmp-conv PRIVATE m ${OpenCL_LIBRARY})
    target_include_directories(bmp-conv PRIVATE ${OpenCL_INCLUDE_DIRS})
endif()

# === Runtime arguments ===
set(INPUT_TF "image2.bmp" CACHE STRING "Input file")
set(FILTER_TYPE "mb" CACHE STRING "Filter type")
set(THREAD_NUM 2 CACHE STRING "Number of threads")
set(COMPUTE_MODE "by_row" CACHE STRING "Compute mode")
set(BLOCK_SIZE 5 CACHE STRING "Block size")
set(OUTPUT_FILE "" CACHE STRING "Output file")
set(LOG 1 CACHE STRING "Enable logging")
set(RWW_MIX "1,1,1" CACHE STRING "Queue RWW mix")
set(MPI_NP 2 CACHE STRING "MPI number of processes")
set(QUEUE_MEM 500 CACHE STRING "Queue memory")
set(QUEUE_CAP 20 CACHE STRING "Queue capacity")
set(VALGRIND_PREFIX "" CACHE STRING "Valgrind prefix (if any)")

# === Helper function to prepend Valgrind ===
function(add_valgrind_prefix CMD)
    if(VALGRIND_PREFIX AND NOT VALGRIND_PREFIX STREQUAL "")
        set(${CMD} "${VALGRIND_PREFIX} ${${CMD}}" PARENT_SCOPE)
    endif()
endfunction()

# === Run targets ===

add_custom_target(run
    COMMAND $<TARGET_FILE:bmp-conv>
            ${INPUT_TF} --filter=${FILTER_TYPE} --threadnum=${THREAD_NUM} --mode=${COMPUTE_MODE} --block=${BLOCK_SIZE} --output=${OUTPUT_FILE} --log=${LOG}
    DEPENDS bmp-conv
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running bmp-conv..."
)

add_custom_target(run-q-mode
    COMMAND $<TARGET_FILE:bmp-conv>
            -queue-mode ${INPUT_TF} --mode=${COMPUTE_MODE} --filter=${FILTER_TYPE} --block=${BLOCK_SIZE} --rww=${RWW_MIX} --queue-size=${QUEUE_CAP} --queue-mem=${QUEUE_MEM}
    DEPENDS bmp-conv
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running Queue Mode..."
)

add_custom_target(run-mac-e-cores
    COMMAND taskpolicy -c background $<TARGET_FILE:bmp-conv>
            ${INPUT_TF} --filter=${FILTER_TYPE} --threadnum=${THREAD_NUM} --mode=${COMPUTE_MODE} --block=${BLOCK_SIZE} --output=${OUTPUT_FILE} --log=${LOG}
    DEPENDS bmp-conv
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running on macOS E-cores..."
)

add_custom_target(run-mac-p-cores
    COMMAND $<TARGET_FILE:bmp-conv>
            ${INPUT_TF} --filter=${FILTER_TYPE} --threadnum=${THREAD_NUM} --mode=${COMPUTE_MODE} --block=${BLOCK_SIZE} --output=${OUTPUT_FILE} --log=${LOG}
    DEPENDS bmp-conv
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running on macOS P-cores..."
)

if(MPI_FOUND)
    add_custom_target(run-mpi-mode
        COMMAND mpirun -np ${MPI_NP} $<TARGET_FILE:bmp-conv>
				-cpu -mpi ${INPUT_TF} --filter=${FILTER_TYPE} --mode=${COMPUTE_MODE} --block=${BLOCK_SIZE} --log=${LOG}
        DEPENDS bmp-conv
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running MPI version..."
    )
endif()
