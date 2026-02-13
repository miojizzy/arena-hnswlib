# CNB Docker 开发环境说明

## 概述

本项目的 CNB 开发环境使用 `Dockerfile.cnb` 构建，不依赖于 `Dockerfile.base`，使用 Ubuntu 24.04 作为基础镜像。

## 环境配置

### 已安装的工具

- **GCC**: 13
- **CMake**: 3.22+
- **Make**: 最新版本
- **Python**: 3.12
- **Go**: 1.23
- **Docker**: 包含 docker CLI 和 docker-compose-plugin
- **Git**: 包含 git-lfs
- **VSCode Server**: 包含 CNB 和 Coding Copilot 扩展
- **Oh My Zsh**: 包含 zsh-autosuggestions 和 zsh-syntax-highlighting 插件

### 基础镜像

- **Ubuntu 24.04**: 不依赖任何自定义基础镜像

## 使用方法

### 本地测试

#### 构建镜像

```bash
docker build -t arena-hnswlib-cnb -f .devcontainer/Dockerfile.cnb .
```

#### 使用脚本运行

```bash
# 进入交互式环境
./scripts/docker-cnb.sh

# 构建项目
./scripts/docker-cnb.sh build

# 运行测试
./scripts/docker-cnb.sh test

# 运行基准测试
./scripts/docker-cnb.sh benchmark

# 一键 CI（构建 + 测试）
./scripts/docker-cnb.sh ci

# 重新构建镜像
./scripts/docker-cnb.sh rebuild

# 清理构建缓存
./scripts/docker-cnb.sh clean

# 运行任意命令
./scripts/docker-cnb.sh bash -c "ls -la"
```

### CNB 平台使用

在 `.cnb.yml` 中，将 `image` 配置注释掉，启用 `build: .devcontainer/Dockerfile.cnb`：

```yaml
$:
  vscode:
    - docker:
        build: .devcontainer/Dockerfile.cnb
        # image: mcr.microsoft.com/devcontainers/universal:linux
      services:
        - vscode
        - docker
      stages:
        - name: say hi
          script: echo "hi"
```

## 特性

1. **独立构建**: 不依赖 `Dockerfile.base`，使用 Ubuntu 24.04 官方镜像
2. **Root 用户运行**: 所有配置和安装在 root 用户下进行
3. **完整工具链**: 包含 C++、Python、Go 开发所需的全部工具
4. **VSCode 扩展**: 预装 CNB welcome 和 Coding Copilot 扩展
5. **Zsh 环境**: 配置了 Oh My Zsh 和常用插件

## 注意事项

- 本环境在 root 用户下运行，适合 CNB 平台的开发环境
- Docker volumes 用于缓存构建产物，提高构建速度
- 所有配置都是独立的，不与原有的 Dockerfile.base 混用
