FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV PYTHON_VERSION=3.11
ENV CONAN_VERSION=2.0.17
ENV GCC_VERSION=13
ENV CMAKE_VERSION=3.28.1

RUN apt-get update && apt-get install -y \
    python${PYTHON_VERSION} \
    python${PYTHON_VERSION}-venv \
    python${PYTHON_VERSION}-dev \
    gcc-${GCC_VERSION} \
    g++-${GCC_VERSION} \
    clang-17 \
    git \
    curl \
    wget \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_VERSION} 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${GCC_VERSION} 100

RUN python${PYTHON_VERSION} -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

RUN pip install --no-cache-dir conan==${CONAN_VERSION}

RUN wget -q https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz \
    && tar -xzf cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz -C /opt \
    && ln -s /opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/cmake /usr/local/bin/cmake \
    && ln -s /opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/ctest /usr/local/bin/ctest \
    && rm cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz

RUN wget -q https://github.com/ninja-build/ninja/releases/download/v1.11.1/ninja-linux.zip \
    && unzip ninja-linux.zip -d /usr/local/bin \
    && rm ninja-linux.zip

RUN wget -q https://github.com/mozilla/sccache/releases/download/v0.7.7/sccache-v0.7.7-x86_64-unknown-linux-musl.tar.gz \
    && tar -xzf sccache-v0.7.7-x86_64-unknown-linux-musl.tar.gz -C /usr/local/bin --strip-components=1 \
    && rm sccache-v0.7.7-x86_64-unknown-linux-musl.tar.gz

RUN conan profile detect --force

WORKDIR /saga

COPY conanfile.py conan.lock BUILD_VERSION ./
COPY profiles/ ./profiles/
COPY cmake/ ./cmake/
COPY CMakeLists.txt CMakePresets.json ./
COPY Engine/ ./Engine/
COPY Backends/ ./Backends/
COPY Runtime/ ./Runtime/
COPY Server/ ./Server/
COPY Editor/ ./Editor/
COPY Main/ ./Main/
COPY Tests/ ./Tests/
COPY Assets/ ./Assets/

ENV SCCACHE_DIR=/saga/cache/sccache
RUN mkdir -p ${SCCACHE_DIR}

RUN conan install . --profile=profiles/linux-gcc-13 --lockfile=conan.lock --build=missing

RUN cmake --preset linux-gcc-13

RUN cmake --build --preset linux-gcc-13

RUN ctest --test-dir Build/linux-gcc-13 --output-on-failure

CMD ["./build.sh", "help"]