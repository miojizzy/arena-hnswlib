#!/bin/bash

# Docker CNB 开发环境脚本
#
# 用法: ./scripts/docker-cnb.sh [命令] [参数...]
#
# 可用命令:
#   (无) / shell   进入交互式开发环境
#   build          构建镜像
#   test           运行测试
#   benchmark      运行基准测试
#   ci             一键构建+测试
#   rebuild        重新构建镜像（修改 Dockerfile 后使用）
#   clean          清理构建缓存和 volumes
#   [其他命令]     在容器中运行任意命令
#
# 示例:
#   ./scripts/docker-cnb.sh                    # 进入交互式 shell
#   ./scripts/docker-cnb.sh build              # 构建镜像
#   ./scripts/docker-cnb.sh test               # 运行测试
#   ./scripts/docker-cnb.sh bash scripts/build.sh  # 运行自定义命令
#   ./scripts/docker-cnb.sh ls -la             # 查看目录
#
# 环境变量:
#   IMAGE_NAME        Docker 镜像名称（默认: arena-hnswlib-cnb）
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

IMAGE_NAME="arena-hnswlib-cnb"
CONTAINER_NAME="arena-hnswlib-cnb-container"

COMMAND=${1:-bash}

# 检查 Docker 是否可用
if ! command -v docker &> /dev/null; then
    echo "❌ Docker 未安装或不在 PATH 中"
    exit 1
fi

# 构建镜像（如果不存在）
if ! docker image inspect "$IMAGE_NAME" &> /dev/null; then
    echo "📦 构建 CNB 镜像..."
    docker build -t "$IMAGE_NAME" -f "$PROJECT_DIR/.devcontainer/Dockerfile.cnb" "$PROJECT_DIR"
fi

case $COMMAND in
    build)
        echo "🔨 构建项目..."
        docker run --rm \
            -v "$PROJECT_DIR:/workspace" \
            -v arena-hnswlib-cnb-build:/workspace/build \
            -v arena-hnswlib-cnb-cmake:/root/.cache \
            "$IMAGE_NAME" \
            bash -c "cmake -B build && cmake --build build --parallel \$(nproc)"
        ;;
    test)
        echo "🧪 运行测试..."
        docker run --rm \
            -v "$PROJECT_DIR:/workspace" \
            -v arena-hnswlib-cnb-build:/workspace/build \
            "$IMAGE_NAME" \
            bash -c "ctest --output-on-failure --test-dir build"
        ;;
    benchmark)
        echo "📊 运行基准测试..."
        docker run --rm \
            -v "$PROJECT_DIR:/workspace" \
            -v arena-hnswlib-cnb-build:/workspace/build \
            "$IMAGE_NAME" \
            bash -c "cd build && ./benchmark_math_utils && ./benchmark_index"
        ;;
    ci)
        echo "🚀 运行 CI (构建 + 测试)..."
        docker run --rm \
            -v "$PROJECT_DIR:/workspace" \
            -v arena-hnswlib-cnb-build:/workspace/build \
            -v arena-hnswlib-cnb-cmake:/root/.cache \
            "$IMAGE_NAME" \
            bash -c "cmake -B build && cmake --build build --parallel \$(nproc) && ctest --output-on-failure --test-dir build"
        ;;
    rebuild)
        echo "🔄 重新构建 CNB 镜像..."
        docker build -t "$IMAGE_NAME" -f "$PROJECT_DIR/.devcontainer/Dockerfile.cnb" "$PROJECT_DIR" --no-cache
        ;;
    clean)
        echo "🧹 清理构建产物..."
        docker volume rm arena-hnswlib-cnb-build arena-hnswlib-cnb-cmake 2>/dev/null || true
        echo "✅ 清理完成"
        ;;
    shell)
        echo "🐳 进入交互式开发环境..."
        docker run --rm -it \
            -v "$PROJECT_DIR:/workspace" \
            -v arena-hnswlib-cnb-build:/workspace/build \
            -v arena-hnswlib-cnb-cmake:/root/.cache \
            "$IMAGE_NAME" \
            /bin/bash
        ;;
    *)
        # 在容器中运行任意命令
        shift 2>/dev/null || true
        if [ -z "$*" ]; then
            # 没有额外参数，进入交互式环境
            exec docker run --rm -it \
                -v "$PROJECT_DIR:/workspace" \
                -v arena-hnswlib-cnb-build:/workspace/build \
                -v arena-hnswlib-cnb-cmake:/root/.cache \
                "$IMAGE_NAME" \
                /bin/bash
        else
            # 运行指定命令
            exec docker run --rm \
                -v "$PROJECT_DIR:/workspace" \
                -v arena-hnswlib-cnb-build:/workspace/build \
                -v arena-hnswlib-cnb-cmake:/root/.cache \
                "$IMAGE_NAME" \
                "$@"
        fi
        ;;
esac
