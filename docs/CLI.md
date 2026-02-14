# Command Line Interface

This document describes all available command-line options for **bmp-conv**.

---

## General Syntax

```bash
./bmp-conv [-cpu|-mpi|-gpu] [--queue-mode] <input.bmp> [options]
```

> **Important:** platform flags (`-cpu`, `-mpi`, `-gpu`) and `--queue-mode` must appear **before** input files.

---

## Platform Modes

| Flag   | Description                                         |
| ------ | --------------------------------------------------- |
| `-cpu` | CPU execution mode (default)                        |
| `-mpi` | MPI-based distributed execution (requires `mpirun`) |
| `-gpu` | GPU mode (WIP)                                      |

---

## Input Files

* One or more `.bmp` files
* In **non-queue mode**, only the first file is used
* In **queue mode**, all files are processed sequentially

---

## Core Options

### `--filter=<type>` (required)

Specifies convolution filter.

### `--mode=<compute_mode>`

Defines workload distribution strategy:

* `by_row`
* `by_column`
* `by_pixel`
* `by_grid`

> Optional when `--threadnum=1`

### `--block=<size>`

Block size for row/column/grid modes.

* `size > 0`
* `1` enables pixel-based processing

---

## Multithreading Options

### `--threadnum=<N>`

Number of worker threads (default: `1`).

### `--queue-mode`

Enables queue-based pipeline processing.
Required for batch input processing.

### `--rww=<r,w,w>`

Number of **reader**, **worker**, and **writer** threads.

* Required in queue mode
* `r + w + w >= 3`

### `--queue-size=<MB>`

Memory limit for queued images (default: `500` MB).

### `--queue-mem=<N>`

Maximum number of queued elements (default: `20`).

---

## Output & Logging

### `--output=<file>`

Output file name.

* Auto-generated if omitted
* Used as template in queue mode

### `--log=<0|1>`

Enable execution logging (default: `0`).

---

## MPI Notes

* Requires `mpich` / `mpich-devel`
* Must be launched via `mpirun`

```bash
mpirun -np 4 ./bmp-conv -mpi image.bmp --filter=em --mode=by_column --block=5
```

