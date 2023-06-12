FROM debian:bullseye-slim
RUN apt-get update && apt-get install -y wget software-properties-common gnupg2 cmake && (wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc) && add-apt-repository -y 'deb http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-16 main' && apt-get update && apt-get -y install libllvm16 llvm-16 llvm-16-dev llvm-16-runtime clang-16 clang-tools-16 libclang-common-16-dev libclang-16-dev libclang1-16 && apt-get autoclean autoremove -y
RUN ln -sf /usr/bin/opt-16 /usr/bin/opt && ln -sf /usr/bin/clang-16 /usr/bin/clang && ln -sf /usr/bin/clang++-16 /usr/bin/clang++
WORKDIR /SheLLVM
COPY . /SheLLVM
RUN mkdir -p build && cd build && cmake -DENABLE_TESTS=ON .. && make -j