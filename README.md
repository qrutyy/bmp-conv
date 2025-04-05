# BMP-CONV

This program applies various filters to a BMP image in a multi-threaded manner, allowing for efficient processing based on the selected filter type and computation mode.

## Usage
At first, you should build the sources by `make build`.

### Command Line Arguments

```bash
Usage: ./bmp-conv [-queue-mode] <input_file.bmp> --filter=<type> [--threadnum=<N>] [--mode=<compute_mode>] [--block=<size>] [--output=<file>] [--log=<N>]
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
- `--block=<size>`: Block size for grid/column/row-based processing. If `1` is chosen – pixel based computation mode will be used. Must be greater than `0`.

[Filter description and performance analysis](https://github.com/qrutyy/bmp-conv/blob/main/MT-mode-analysis.md)

#### Optional Arguments:
- `-queue-mode` : Enables queue-based multi-threaded mode. Implemented for multiple input files processing. **Should be the first argumen** (made for better args handling)
- `--threadnum=<N>`: The number of threads to use (default: `1`). Specify the number of threads to run in parallel. **Isn't required for queue mode.**
- `--output=<file>`: The name of the output file where the processed image will be saved. If not specified, an output filename will be generated based on the input filename. In case of queue-mode provided parameter will be used as a template
- `--log=<0|1>`: Enable or disable logging (default: `0`). Set to `1` to log execution time and parameters to a file.
- `--mode=<compute_mode>`: This argument is optional when threadnum equals to `1` (single-threaded mode is turned on).
- `--rww=<x,y,z>`: Sets the number of **reader**, **worker** and **writer** threads. Is required in queue-mode. If `sum < 3` -> queue-mode won't work.  
- `--lim=<N>`: Sets the memory limit (in MB) for images being queued for convolution. Initial value is `500` MB. 

### Example Usage

Apply Sharpen filter in single-threded (sequential) column mode with block size of 30 (minimum args required):
```bash
./src/bmp-conv image1.bmp --mode=by_column --filter=sh --block=30
```

Apply Big Gaussian Blur in grid mode using 4 threads and block size of 16 (+ specified output):
```bash
./src/bmp-conv image5.bmp --mode=by_grid --filter=gg --threadnum=4 --block=16 --output=output.bmp
```

Apply Box Blur in multi-threaded queue-based mode (1 reader and writer thread, 2 worker threads): 
```bash
./src/bmp-conv -queue-mode image4.bmp image4.bmp image4.bmp --mode=by_row --filter=bb --block=5 --rww=1,2,1
```
add queue-mode example

### Testing
For future performance analysis of mutlithreaded mode - shell script for benchmarking and plot gen were implemented. To execute tests - simply run:
```
./tests/benchmark.sh
``` 
To use it only as a testing-system (without plot generation) - use `-v|--verify` option.

**(WIP)** To test the queued mode and its balancing - use:
```
./tests/q-mode-benchmark.sh
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
, where **LOG_TAG = <QPOP|QPUSH|READER|WRITER|WORKER>**
## License

Distributed under the [GPL-3.0 License](https://github.com/qrutyy/bmp-conv/blob/main/LICENSE). 

