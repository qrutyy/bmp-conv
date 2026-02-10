# Architecture Overview

This document describes internal execution models and parallelization strategies used in **bmp-conv**.

---

## High-Level Design

The system processes BMP images using configurable convolution filters and supports multiple execution backends:

* Single-threaded CPU
* Multi-threaded CPU (static partitioning)
* Queue-based pipeline (R/W/W)
* MPI-based distributed execution

---

## CPU Execution Models

### Single-threaded

* Default execution mode
* Used when `--threadnum=1`
* Deterministic baseline for performance comparison

---

### Static Multi-threading

* Image is partitioned according to `--mode`
* Each thread processes a fixed region

Partition strategies:

* **by_row** — contiguous row blocks
* **by_column** — contiguous column blocks
* **by_grid** — 2D block partitioning
* **by_pixel** — fine-grained distribution

---

## Queue-Based Pipeline Model

Enabled via `--queue-mode`.

### Thread Roles

| Role   | Responsibility        |
| ------ | --------------------- |
| Reader | Load images from disk |
| Worker | Apply convolution     |
| Writer | Save processed images |

Images move through bounded queues with configurable memory limits.

### Advantages

* Overlaps I/O and computation
* Efficient for batch workloads
* Scales better with heterogeneous tasks

---

## MPI Execution Model

* Each process handles a partition of the image
* Root process distributes metadata
* Partial results are gathered and merged

Supported distribution modes:

* `by_row`
* `by_column`

MPI is primarily used for **scalability experiments** and distributed performance evaluation.

---

## Logging & Instrumentation

* Execution time per run
* Queue blocking time (`QPUSH`, `QPOP`)
* Per-thread role timing

Used for performance profiling and benchmarking.

---

## Design Goals

* Compare parallel computation strategies
* Study synchronization overhead
* Evaluate scalability on multi-core and multi-node systems
