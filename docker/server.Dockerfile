# Pinned LLVM 18 build stage.  The final image contains only the server and
# its LLVM runtime libraries, not CMake, Ninja, or a C++ toolchain.
FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates cmake ninja-build clang-18 llvm-18-dev \
    libedit-dev libxml2-dev libzstd-dev zlib1g-dev && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY compiler ./compiler
COPY tests ./tests
COPY tools ./tools
COPY examples ./examples
# The install rule packages both the CLI and service targets.
RUN cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang-18 \
    -DCMAKE_CXX_COMPILER=clang++-18 \
    -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm \
    -DNANO_BUILD_TESTS=OFF -DNANO_BUILD_WEB_SERVER=ON -DNANO_BUILD_JIT=OFF && \
    cmake --build build --target nano nano-server -j2 && \
    cmake --install build --prefix /opt/nanopass

FROM ubuntu:24.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates wget libllvm18 llvm-18-runtime && \
    rm -rf /var/lib/apt/lists/*
RUN useradd --system --create-home --uid 10001 nanopass
COPY --from=build /opt/nanopass /opt/nanopass
USER nanopass
EXPOSE 8080
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
  CMD /bin/sh -c "wget -q -O - http://127.0.0.1:8080/api/v1/readiness | grep -q '\"ready\":true'"
ENTRYPOINT ["/opt/nanopass/bin/nano-server", "--bind=all"]
