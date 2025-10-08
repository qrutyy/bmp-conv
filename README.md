# BMP-CONV

This tool provides a various range of computing approaches for BMP image convolution algorithm. It includes singlethreaded, multithreaded (queued and static) and MPI-based approaches. Convolution can be used with multiple different filters and configuration options. The analysis of proposed approaches along with some notes is presented in [docs](https://github.com/qrutyy/bmp-conv/tree/main/docs).

## Usage
At first, you should build the sources by `make`.

### Command Line Arguments

```bash
Usage: ./bmp-conv [-queue-mode/-mpi-mode] <input_file.bmp> --filter=<type> [--threadnum=<N>] [--mode=<compute_mode>] [--block=<size>] [--output=<file>] [--log=<N>]
```

#### Mandatory Arguments:
- `<input_file.bmp>`: The path to the input BMP image from test-img/ dir.  **All free arguments will be considered as input_files. Non queued mode uses only 1 input file.**
- `--mode=<compute_mode>`: Defines the mode of computation. Possible values:
  - `by_row`: Process image row by row.
  - `by_column`: Process image column by column.
  - `by_pixel`: Process image pixel by pixel.
  - `by_grid`: Process image by dividing it into blocks (grid mode).
- `--filter=<type>`: Specifies the filter to be applied. Valid filter types:
  - `bb`: Blur
  - `mb`: Motion Blur
  - `em`: Emboss
  - `gg`: Gaussian Blur (Big)
  - `gb`: Gaussian Blur
  - `co`: Convolution
  - `sh`: Sharpen
  - `mm`: Median
  - `bo`: Box Blur
  - `mg`: Median Gaussian
- `--block=<size>`: Block size for grid/column/row-based processing. If `1` is chosen – pixel based computation mode will be used. **Must be greater than `0`**.

[Filter description and MT-mode performance analysis](https://github.com/qrutyy/bmp-conv/blob/main/MT-mode-analysis.md)

#### Optional Arguments:
- `-queue-mode` : Enables queue-based multi-threaded mode. Implemented for multiple input files processing. **Should be the first argumen** (made for better args handling)
- `-mpi-mode` : Enables MPI-based mode. Implemented for using processing with multiple processes. **Should be the first argumen** (made for better args handling). `mpich` and `mpich-devel` packages are required. **Should be ran with `mpirun` - see usage examples**.
- `--threadnum=<N>`: The number of threads to use (default: `1`). Specify the number of threads to run in parallel. **Isn't required for queue mode.**
- `--output=<file>`: The name of the output file where the processed image will be saved. If not specified, an output filename will be generated based on the input filename. In case of queue-mode provided parameter will be used as a template
- `--log=<0|1>`: Enable or disable logging (default: `0`). Set to `1` to log execution time and parameters to a file.
- `--mode=<compute_mode>`: This argument is optional when threadnum equals to `1` (single-threaded mode is turned on).
- `--rww=<x,y,z>`: Sets the number of **reader**, **worker** and **writer** threads. Is required in queue-mode. If `sum < 3` -> queue-mode won't work.  
- `--queue-size=<N>`: Sets the memory limit (in MB) for images being queued for convolution. Initial value is `500` MB. 
- `--queue-mem=<N>`: Sets the queue's element limit. Initial value is `20` 

### Usage Examples

Apply Sharpen filter in single-threded (sequential) column mode with block size of 30 (minimum args required):
```bash
./bmp-conv image1.bmp --mode=by_column --filter=sh --block=30
```

Apply Big Gaussian Blur in grid mode using 4 threads and block size of 16 (+ specified output):
```bash
./bmp-conv image5.bmp --mode=by_grid --filter=gg --threadnum=4 --block=16 --output=output.bmp
```

Apply Box Blur in multi-threaded queue-based mode using "by_row" distribution (1 reader and writer thread, 2 worker threads): 
```bash
./bmp-conv -queue-mode image1.bmp image2.bmp image3.bmp --mode=by_row --filter=bb --block=5 --rww=1,2,1
```

Apply Emboss filter in MPI mode using "by_column" distribution (4 processes):
```bash
mpirun -np 4 ./bmp-conv-mpi -mpi-mode image5.bmp --mode=by_column --filter=em --block=5 
```

*Additionally, to shorten the calls - check Makefile targets (etc. run, run-mpi-mode...)*

### Testing
For future performance analysis of mutlithreaded mode/queue-mode/mpi-mode - shell scripts for benchmarking and plot genrerators were implemented. At first - install dependencies (see [Benchmark-setup](https://github.com/qrutyy/bmp-conv/blob/main/Benchmark-setup.md))To execute tests - simply run:
```
./tests/st-mt-benchmark.sh
./tests/qmt-benchmark.sh
./tests/mpi-benchmark.sh
``` 

To test the correctness of multithreaded part - use (add options for better dynamic analysis, see *workflows/analysis.yml*):
```
./tests/test.sh [-ci] [-ci-memcheck] [-ci-helgrind]
```

### Logs
If logging is enabled, execution times and configurations (filter, thread count, block size) will be saved in a log file, `.txt`. The format is:
```
<RunID> <filter_type> <threadnum> <mode> <block_size> <execution_time>
```
Queued mode logging saves blocking time for `queue_pop` and `queue_push`, and execution_time for every thread of every role. The format is:
```
<LOG_TAG> <TIME>
```
, where **LOG_TAG = < QPOP | QPUSH | READER | WRITER | WORKER >**
## License

Distributed under the [GPL-3.0 License](https://github.com/qrutyy/bmp-conv/blob/main/LICENSE). 

