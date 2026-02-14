#!/bin/bash
mpirun \
  --hostfile mpi/hosts \
  -np 5 \
  build/bmp-conv "$@"
