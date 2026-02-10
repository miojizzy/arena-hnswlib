#!/bin/bash

# Docker 开发环境脚本（不需要 docker-compose）
#
# 用法: ./scripts/docker-dev.sh [命令] [参数...]
#
# 可用命令:
#   (无) / shell   进入交互式开发环境
#   build          构建项目
#   test           运行测试
#   benchmark      运行基准测试
#   ci             一键构建+测试
#   rebuild        重新构建镜像（修改 Dockerfile 后使用）
#   clean          清理构建缓存和 volumes
#   [其他命令]     在容器中运行任意命令
#
# 示例:
#   ./scripts/docker-dev.sh                    # 进入交互式 shell
#   ./scripts/docker-dev.sh build              # 构建项目
#   ./scripts/docker-dev.sh test               # 运行测试
#   ./scripts/docker-dev.sh bash scripts/build.sh  # 运行自定义命令
#   ./scripts/docker-dev.sh ls -la             # 查看目录
#
# 环境变量:
#   IMAGE_NAME        Docker 镜像名称（默认: arena-hnswlib-dev）
#
# 注意: 
#   - 首次运行会自动构建 Docker 镜像
#   - 使用 Docker volumes 缓存构建产物，加速重复构建
#   - 源代码通过 volume 挂载，支持实时编辑

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

IMAGE_NAME="arena-hnswlib-dev"
CONTAINER_NAME="arena-hnswlib-container"

COMMAND=${1:-bash}

# 检查 Docker 是否可用
if ! command -v docker &> /dev/null; then
    echo "❌ Docker 未安装或不在 PATH 中"
    exit 1
fi

# 构建镜像（如果不存在）
if ! docker image inspect "$IMAGE_NAME" &> /dev/null; then
    echo "📦 构建镜像..."
    docker build -t "$IMAGE_NAME" "$PROJECT_DIR"
fi

case $COMMAND in
    build)
        echo "🔨 构建项目..."
        docker run --rm \
            -v "$PROJECT_DIR:/workspace" \
            -v arena-hnswlib-build:/workspace/build \
            -v arena-hnswlib-cmake:/root/.cache \
            "$IMAGE_NAME" \
            bash -c "cmake -B build && cmake --build build --parallel \$(nproc)"
        ;;
    test)
        echo "🧪 运行测试..."
        docker run --rm \
            -v "$PROJECT_DIR:/workspace" \
            -v arena-hnswlib-build:/workspace/build \
            "$IMAGE_NAME" \
            bash -c "ctest --output-on-failure --test-dir build"
        ;;
    benchmark)
        echo "📊 运行基准测试..."
        docker run --rm \
            -v "$PROJECT_DIR:/workspace" \
            -v arena-hnswlib-build:/workspace/build \
            "$IMAGE_NAME" \
            bash -c "cd build && ./benchmark_math_utils && ./benchmark_index"
        ;;
    ci)
        echo "🚀 运行 CI (构建 + 测试)..."
        docker run --rm \
            -v "$PROJECT_DIR:/workspace" \
            -v arena-hnswlib-build:/workspace/build \
            -v arena-hnswlib-cmake:/root/.cache \
            "$IMAGE_NAME" \
            bash -c "cmake -B build && cmake --build build --parallel \$(nproc) && ctest --output-on-failure --test-dir build"
        ;;
    rebuild)
        echo "🔄 重新构建镜像..."
        docker build -t "$IMAGE_NAME" "$PROJECT_DIR" --no-cache
        ;;
    clean)
        echo "🧹 清理构建产物..."
        docker volume rm arena-hnswlib-build arena-hnswlib-cmake 2>/dev/null || true
        echo "✅ 清理完成"
        ;;
    shell)
        echo "🐳 进入交互式开发环境..."
        docker run --rm -it \
            -v "$PROJECT_DIR:/workspace" \
            -v arena-hnswlib-build:/workspace/build \
            -v arena-hnswlib-cmake:/root/.cache \
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
                -v arena-hnswlib-build:/workspace/build \
                -v arena-hnswlib-cmake:/root/.cache \
                "$IMAGE_NAME" \
                /bin/bash
        else
            # 运行指定命令
            exec docker run --rm \
                -v "$PROJECT_DIR:/workspace" \
                -v arena-hnswlib-build:/workspace/build \
                -v arena-hnswlib-cmake:/root/.cache \
                "$IMAGE_NAME" \
                "$@"
        fi
        ;;
esac
