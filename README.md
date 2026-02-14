# bmp-conv

High-performance BMP image convolution tool with multiple parallelization strategies.

`bmp-conv` explores different computation models for image convolution:
- on-CPU (single and multi-threaded) (static & queue-based)
- MPI-based distributed processing
- on-GPU WIP

## Features

- CPU, MPI (GPU — WIP)
- Multiple convolution filters (blur, sharpen, gaussian, median, etc.)
- Several workload distribution strategies
- Queue-based pipeline for batch processing
- Built-in benchmarking & logging

## Quick Start

### Build
```bash
cmake -S . -B build
cmake --build build
````

### Minimal usage

```bash
./bmp-conv image.bmp --filter=sh --mode=by_column --block=30
```

## Execution Modes

### Platforms

| Mode   | Description                                |
| ------ | ------------------------------------------ |
| `-cpu` | Default CPU execution                      |
| `-mpi` | Distributed execution using MPI (`mpirun`) |
| `-gpu` | GPU mode (work in progress)                |

### Distribution strategies

* `by_row`
* `by_column`
* `by_pixel`
* `by_grid` (block-based)

## Examples

**Multi-threaded grid convolution**

```bash
./bmp-conv -cpu image.bmp --filter=gg --mode=by_grid --threadnum=4 --block=16
```

**Queue-based batch processing**

```bash
./bmp-conv -queue-mode img1.bmp img2.bmp img3.bmp \
  --filter=bb --mode=by_row --block=5 --rww=1,2,1
```

**MPI execution (4 processes)**

```bash
mpirun -np 4 ./bmp-conv-mpi -mpi image.bmp \
  --filter=em --mode=by_column --block=5
```


## Benchmarking & Analysis

The project includes scripts for:

* single vs multi-thread comparison
* queue-based pipeline analysis
* MPI scalability

See:

* [`docs/MT-mode-analysis.md`](docs/MT-mode-analysis.md)
* [`docs/Benchmark-setup.md`](docs/Benchmark-setup.md)

Run benchmarks:

```bash
./tests/st-mt-benchmark.sh
./tests/qmt-benchmark.sh
./tests/mpi-benchmark.sh
```


## Documentation

* [Command-line reference](docs/CLI.md)
* [Filters overview](docs/Filters.md)
* [Parallel models explained](docs/Architecture.md)
* [Performance analysis](docs/MT-mode-analysis.md)


## License

GPL-3.0 — see [LICENSE](LICENSE)
