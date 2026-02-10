FROM ubuntu:22.04

# 避免交互式提示
ENV DEBIAN_FRONTEND=noninteractive

# 设置工作目录
WORKDIR /workspace

# 安装基本工具
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    && rm -rf /var/lib/apt/lists/*

# 预缓存 CMake 依赖（通过构建空项目）
COPY CMakeLists.txt /workspace/
RUN cmake -B /tmp/build -S /workspace 2>&1 || true

# 复制整个项目
COPY . /workspace/

# 默认命令
CMD ["/bin/bash"]
