# syntax=docker/dockerfile:1

ARG UBUNTU_VERSION=24.04

FROM ubuntu:${UBUNTU_VERSION} AS toolchain

ARG DEBIAN_FRONTEND=noninteractive
ARG JOBS=4

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ENV TZ=Etc/UTC \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

# Compiler construction, cross-compilation, emulation, debugging, linting,
# IR inspection, CFG rendering, and benchmark utilities used by this project.
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    rm -f /etc/apt/apt.conf.d/docker-clean \
    && apt-get -o Acquire::Retries=5 update \
    && apt-get -o Acquire::Retries=5 install -y --no-install-recommends \
        bc \
        binutils-aarch64-linux-gnu \
        binutils-riscv64-linux-gnu \
        binutils-riscv64-unknown-elf \
        bear \
        bison \
        build-essential \
        ca-certificates \
        clang \
        clangd \
        clang-format \
        clang-tidy \
        clang-tools \
        cmake \
        curl \
        file \
        flex \
        g++-aarch64-linux-gnu \
        g++-riscv64-linux-gnu \
        gcc-aarch64-linux-gnu \
        gcc-riscv64-linux-gnu \
        gcc-riscv64-unknown-elf \
        gdb-multiarch \
        git \
        graphviz \
        jq \
        less \
        libc6-dev-arm64-cross \
        libc6-dev-riscv64-cross \
        libfl-dev \
        lld \
        llvm \
        make \
        ninja-build \
        pkg-config \
        python3 \
        python3-pip \
        python3-venv \
        qemu-system-common \
        qemu-user \
        time \
        valgrind \
        vim-tiny \
        wget

# Fail during image creation if any command expected by the Makefile or test
# harness is absent.
RUN required_tools=( \
        aarch64-linux-gnu-ar aarch64-linux-gnu-gcc \
        bear bison clang clang++ clangd clang-format clang-tidy dot flex gcc gdb-multiarch \
        llvm-mca make opt python3 qemu-aarch64 qemu-riscv64 \
        riscv64-linux-gnu-ar riscv64-linux-gnu-gcc \
        riscv64-unknown-elf-ar riscv64-unknown-elf-gcc valgrind \
    ) \
    && for tool in "${required_tools[@]}"; do command -v "${tool}"; done

FROM toolchain AS development

ARG JOBS=4
ARG RUN_DIRECTED_TESTS=0

WORKDIR /workspace
COPY . .

# Compile the compiler and all three SysY runtime libraries as an image-level
# smoke check. QEMU uses Linux-targeted static runtimes for both guest targets.
RUN make -j"${JOBS}" build \
    && make libx86 libarm librv \
        RISCV_GCC=riscv64-linux-gnu-gcc \
        RISCV_AR=riscv64-linux-gnu-ar \
    && if [[ "${RUN_DIRECTED_TESTS}" = "1" ]]; then \
         python3 script/directed/directed.py; \
       fi

CMD ["/bin/bash"]
