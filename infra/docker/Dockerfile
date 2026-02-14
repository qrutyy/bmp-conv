FROM ubuntu:latest

RUN apt-get update && apt-get install --no-install-recommends -y \
    build-essential \
    cmake \
    mpich \
    libmpich-dev \
	ocl-icd-opencl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -S . -B build \
 && cmake --build build

CMD ["sleep", "infinity"]
