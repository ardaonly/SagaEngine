FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV CONAN_VERSION=2.0.17

RUN apt-get update && apt-get install -y \
    python3 \
    python3-venv \
    python3-dev \
    build-essential \
    gcc \
    g++ \
    cmake \
    ninja-build \
    sccache \
    git \
    curl \
    wget \
    unzip \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

RUN pip install --no-cache-dir conan==${CONAN_VERSION}
RUN conan profile detect --force

WORKDIR /saga

COPY conanfile.py VERSION version.json ./
COPY profiles/ ./profiles/
COPY cmake/ ./cmake/
COPY CMakeLists.txt CMakePresets.json ./
COPY Shared/ ./Shared/
COPY Collaboration/ ./Collaboration/
COPY Engine/ ./Engine/
COPY Backends/ ./Backends/
COPY Runtime/ ./Runtime/
COPY Server/ ./Server/
COPY Editor/ ./Editor/
COPY Apps/ ./Apps/
COPY Tests/ ./Tests/

ENV SCCACHE_DIR=/saga/cache/sccache
RUN mkdir -p ${SCCACHE_DIR}

RUN conan install . \
    --profile:host=default \
    --profile:build=default \
    -s build_type=RelWithDebInfo \
    --build=missing

RUN cmake --preset linux-gcc
RUN cmake --build --preset linux-gcc --target SagaServer --parallel

EXPOSE 7777/udp
ENTRYPOINT ["/saga/build/RelWithDebInfo/SagaServer"]
CMD ["--port", "7777"]
