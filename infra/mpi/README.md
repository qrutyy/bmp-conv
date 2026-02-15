```bash
# Build and start all 5 nodes (1 head + 4 workers)
docker compose -f infra/mpi/docker-compose.yml up --build -d

# Run MPI across all 5 containers (one rank per node). Use the hostfile so ranks run on each host.
docker exec -it mpi-head_node-1 mpirun -f infra/mpi/hosts -np 5 --bind-to none /app/build/bmp-conv -cpu -mpi --filter=em --block=5 --mode=by_row image6.bmp

# Or get a shell on the head node and run mpirun there (e.g. to use a local input file)
docker exec -it mpi-head_node-1 sh
mpirun -f infra/mpi/hosts -np 5 --bind-to none /app/build/bmp-conv -cpu -mpi --filter=em --block=5 --mode=by_row image6.bmp

# Tear down
docker compose -f infra/mpi/docker-compose.yml down
```

**Notes:**

- Use `-hostfile /app/infra/mpi/hosts` so that `mpirun` starts one process on each of the five containers. Without it, all processes would run on the head node and MPI would not span containers.
- The binary must be at the same path on every node; do not mount a volume over `/app` on the head node or the workers will have the binary but the head will not and `mpirun` will fail.
- If you still see each process reporting `Size: 1`, rebuild the image after the Dockerfile/README changes and ensure no volume overrides `/app` on the head.
- Do not use `docker-compose scale` for scaling the ammount of rank N nodes, ftm theyre hard-coded in docker-compose.
