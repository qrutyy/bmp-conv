mpirun \
  -n 5 \
  -hostfile infra/mpi \
  build/bmp-conv -mpi image6.bmp --filter=em --mode=by_column --block=5
