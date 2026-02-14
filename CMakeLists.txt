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

# === MPI ===
find_package(MPI)
find_package(OpenCL REQUIRED)

# === Sources ===
set(SRCS_COMMON
    src/bmp-conv.c
    src/utils/args-parse.c
    src/utils/filters.c
    src/utils/threads-general.c
    src/utils/utils.c
    src/backend/compute-backend.c
    src/backend/cpu/cpu-backend.c
    src/backend/cpu/st/st_exec.c
    src/backend/cpu/mt/mt_compute.c
    src/backend/cpu/mt/mt_exec.c
    src/backend/cpu/qmt/qmt_exec.c
    src/backend/cpu/qmt/qmt_queue.c
    src/backend/cpu/qmt/qmt_threads.c
	src/backend/gpu/gpu-backend.c
	src/backend/gpu/core/mw-exec.c
	src/backend/gpu/utils/utils.c
    logger/log.c
    libbmp/libbmp.c
)

set(SRCS_MPI_ONLY
    src/backend/mpi/mpi-backend.c
    src/backend/mpi/core/exec.c
    src/backend/mpi/utils/utils.c
    src/backend/mpi/comm/phases.c
    src/backend/mpi/comm/rank0-proc.c
    src/backend/mpi/comm/data-transfer.c
    src/backend/mpi/compute/filter-comp.c
)

# === Executables ===
add_executable(bmp-conv ${SRCS_COMMON})
target_link_libraries(bmp-conv PRIVATE m)

if(MPI_FOUND)
	message(STATUS "Found MPI: ${MPI}")
    add_executable(bmp-conv-mpi ${SRCS_COMMON} ${SRCS_MPI_ONLY})
    target_compile_definitions(bmp-conv-mpi PRIVATE USE_MPI)
    target_link_libraries(bmp-conv-mpi PRIVATE MPI::MPI_C m ${OpenCL_LIBRARY})
endif()

if(OpenCL_FOUND)
    message(STATUS "Found OpenCL: ${OpenCL_LIBRARY}")
    include_directories(${OpenCL_INCLUDE_DIRS})
	target_link_libraries(bmp-conv PRIVATE ${OpenCL_LIBRARY} m)
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
        COMMAND mpirun -np ${MPI_NP} $<TARGET_FILE:bmp-conv-mpi>
                -mpi ${INPUT_TF} --filter=${FILTER_TYPE} --mode=${COMPUTE_MODE} --block=${BLOCK_SIZE} --log=${LOG}
        DEPENDS bmp-conv-mpi
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running MPI version..."
    )
endif()
