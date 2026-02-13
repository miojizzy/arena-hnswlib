# .configs 配置目录说明

本目录集中管理项目的所有配置文件（dotfiles），涵盖开发环境、CI/CD 流水线、AI 助手等配置。

## 目录结构

```
.configs/
├── devcontainer/          # GitHub Codespaces 开发环境
├── ide/                   # CNB Workspace IDE 配置
├── github/                # GitHub 相关配置（源）
│   ├── workflows/         # GitHub Actions 工作流
│   ├── agents/            # GitHub Copilot Agent 配置
│   ├── instructions/      # GitHub Copilot 指令
│   └── skills/            # GitHub Copilot 技能
├── codebuddy/             # CNB Codebuddy 配置（符号链接）
│   ├── agents -> ../github/agents
│   ├── instructions -> ../github/instructions
│   └── skills -> ../github/skills
├── cnb.yml                # CNB 平台配置（事件触发 + 双向同步）(已转移到根目录)
└── CNB_DOCKER_README.md   # CNB Docker 环境详细说明
```

## 开发环境

### GitHub Codespaces

使用 [devcontainer](./devcontainer/devcontainer.json) 构建开发环境：

- 基于 `mcr.microsoft.com/devcontainers/universal:linux` 镜像
- 启动位置：`.configs/devcontainer/`（通过根目录符号链接引用）

### CNB Workspace

使用 `.cnb.yml` 和 `.ide/Dockerfile` 构建开发环境：

- **配置文件**: [.cnb.yml](已直接放置在根目录) 定义开发环境构建和服务配置（CNB 平台不支持软链接配置文件）
- **Docker 镜像**: [ide/Dockerfile](./ide/Dockerfile) 基于 Ubuntu 24.04
- **详细说明**: 参见 [CNB_DOCKER_README.md](./CNB_DOCKER_README.md)

预装工具包括：GCC 13、CMake 3.22+、Python 3.12、Go 1.23、Docker、Git LFS、Oh My Zsh 等。

## CI/CD 与事件同步

### GitHub Actions

位于 [github/workflows/](./github/workflows/)：

| 工作流 | 触发条件 | 说明 |
|--------|----------|------|
| `ci.yml` | push/PR 到 main | 构建、测试流水线 |
| `sync-to-cnb.yml` | push 到 develop | 同步代码到 CNB |

### CNB 事件

在 [cnb.yml](./cnb.yml) 的 `develop.push` 中定义：

- 收到 push 时触发同步到 GitHub
- 通过 `[sync]` 前缀识别同步提交，避免循环触发

### 双向同步机制

```
GitHub (develop branch)  ──sync-to-cnb.yml──>  CNB
         ↑                                           │
         │                                           │
         └────────── cnb.yml (develop.push) ─────────┘
```

- **GitHub → CNB**: 通过 `sync-to-cnb.yml` 工作流
- **CNB → GitHub**: 通过 `cnb.yml` 中的 `develop.push` 事件
- **防循环**: 使用 `[sync]` 前缀标记同步提交，双向都会检查并跳过

## AI 助手配置

### GitHub Copilot

配置存储在 [github/](./github/) 目录：

- `agents/` - Agent 定义文件
- `instructions/` - 项目约定和指令
- `skills/` - 可复用的技能模块

### CNB Codebuddy

通过符号链接复用 GitHub Copilot 配置：

```bash
codebuddy/agents     -> ../github/agents
codebuddy/instructions -> ../github/instructions  
codebuddy/skills     -> ../github/skills
```

**设计理念**: 一份配置，双平台复用，减少维护成本。

## 使用指引

### 在 GitHub Codespaces 中开发

1. 在 GitHub 仓库页面点击 "Code" → "Codespaces" → "Create codespace"
2. 自动使用 `.configs/devcontainer/devcontainer.json` 构建环境

### 在 CNB Workspace 中开发

1. 创建 CNB Workspace，关联项目仓库
2. 自动使用 `.configs/cnb.yml` 和 `.configs/ide/Dockerfile` 构建环境
3. 详细配置参见 [CNB_DOCKER_README.md](./CNB_DOCKER_README.md)

### 修改 AI 助手配置

在 `.configs/github/` 下修改，CNB 环境会自动同步：

- 新增指令 → `github/instructions/`
- 新增技能 → `github/skills/`
- 新增 Agent → `github/agents/`
