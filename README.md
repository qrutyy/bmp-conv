# BMP-CONV

This program applies various filters to a BMP image in a multi-threaded manner, allowing for efficient processing based on the selected filter type and computation mode.

## Usage
First, you should build the sources by `make build`.

### Command Line Arguments

```bash
Usage: ./bmp-conv <input_file.bmp> [--mode=<compute_mode>] --filter=<type> --threadnum=<N> [--block=<size>] [--output=<file>] [--log=<N>]
```

#### Required Arguments:
- `<input_file.bmp>`: The path to the input BMP image.
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

#### Optional Arguments:
- `--threadnum=<N>`: The number of threads to use (default: `1`). Specify the number of threads to run in parallel.
- `--block=<size>`: Block size for grid/column/row-based processing. Must be greater than `0`.
- `--output=<file>`: The name of the output file where the processed image will be saved. If not specified, an output filename will be generated based on the input filename.
- `--log=<0|1>`: Enable or disable logging (default: `0`). Set to `1` to log execution time and parameters to a file.
- `--mode=<compute_mode>`: This argument is optional when threadnum == 1 (single-threaded mode is turned on).

### Example Usage

**Apply Gaussian Blur in grid mode using 4 threads and block size of 16:**
   ```bash
   ./image_filter input.bmp --mode=by_grid --filter=gg --threadnum=4 --block=16 --output=output.bmp
   ```

#### Logs
If logging is enabled, execution times and configurations (filter, thread count, block size) will be saved in a log file, `log.txt`. The format is:
```
<RunID> <filter_type> <threadnum> <mode> <block_size> <execution_time>
```
## License

Distributed under the [GPL-3.0 License](https://github.com/qrutyy/bmp-conv/blob/main/LICENSE). 

