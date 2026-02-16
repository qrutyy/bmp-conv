FROM ubuntu:22.04

# 1. Install dependencies
RUN apt-get update && apt-get install --no-install-recommends -y \
    build-essential \
    cmake \
    mpich \
    libmpich-dev \
    ocl-icd-opencl-dev \
    openssh-server \
    openssh-client \
    iproute2 \
    && rm -rf /var/lib/apt/lists/*

# 2. Setup SSH Server
RUN mkdir /var/run/sshd
# Set root password to allow login
RUN echo 'root:root' | chpasswd

# Allow Root Login and Disable PAM (prevents environment variable stripping)
RUN sed -i 's/^#*PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config && \
    sed -i 's/^#*UsePAM.*/UsePAM no/' /etc/ssh/sshd_config

# 3. Setup SSH Keys
RUN mkdir -p /root/.ssh && \
    ssh-keygen -t rsa -N "" -f /root/.ssh/id_rsa && \
    cat /root/.ssh/id_rsa.pub >> /root/.ssh/authorized_keys && \
    chmod 600 /root/.ssh/authorized_keys

# 4. SSH Client Config
RUN printf "Host *\n\
    StrictHostKeyChecking no\n\
    UserKnownHostsFile /dev/null\n\
    LogLevel QUIET\n" > /root/.ssh/config && \
    chmod 600 /root/.ssh/config

# Force MPICH to use the docker network interface, not localhost
ENV HYDRA_IFACE=eth0
# Disable strict checking for root execution (optional but helps some versions)
ENV MPIRUN_FLAGS="--allow-run-as-root"

WORKDIR /app
COPY . .

# 5. Build
RUN cmake -S . -B build && cmake --build build

EXPOSE 22
CMD ["/usr/sbin/sshd", "-D"]
