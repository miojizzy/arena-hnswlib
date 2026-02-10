# CodeBuddy 配置指南

本目录包含 codebuddy 的项目配置。

## 配置文件

### `config.json`
主配置文件，定义了 codebuddy 如何使用项目中的 instructions。

## 配置项说明

### Instructions (指令)
- **enabled**: `true` - 启用指令功能
- **paths**: `[".github/instructions/"]` - 指令文件所在目录
- **defaultStrategy**: `"merge"` - 多个指令合并策略
- **autoLoad**: `true` - 自动加载指令

### Memory Bank (记忆库)
- **enabled**: `true` - 启用记忆库功能
- **path**: `"memory-bank/"` - 记忆库文件所在目录
- **autoLoad**: `true` - 自动加载记忆库

### Agents (智能体)
- **enabled**: `true` - 启用智能体功能
- **path**: `".github/agents/"` - 智能体定义文件所在目录
- **autoDiscover**: `true` - 自动发现智能体文件
- **filePattern**: `"*.agent.md"` - 智能体文件模式

## CNB Workspace 配置

对于 CNB (Cloud Native Build) 在线 workspace，codebuddy 可能需要额外的配置文件：

### 1. VS Code 设置
- **文件**: `.vscode/settings.json`
- **作用**: 为 CNB workspace 中的 VS Code 配置 Copilot 和 agent 设置

### 2. GitHub Copilot 配置
- **文件**: `.copilot-instructions.json` (项目根目录)
- **文件**: `.github/copilot/instructions.md`
- **作用**: GitHub Copilot 标准配置位置

### 3. Agent 配置
- **位置**: `.github/agents/*.agent.md`
- **格式**: 标准 GitHub Copilot agent 格式，包含 frontmatter 配置

## CNB Workspace 注意事项

CNB workspace 中的 codebuddy 可能需要：
1. **重启 workspace** 使新配置生效
2. **检查权限** 确保 codebuddy 可以读取 `.github/agents/` 目录
3. **验证格式** 确保 agent 文件符合 CNB codebuddy 的格式要求
4. **查看日志** 如有问题，查看 CNB workspace 的日志输出

## 当前可用的 Instructions

### 1. Memory Bank Instructions (`memory-bank.instructions.md`)
- 应用范围: 所有文件 (`**`)
- 功能: 管理项目记忆、任务跟踪、上下文维护
- 使用场景: 每次任务开始时自动读取记忆库

### 2. Agent Skills Instructions (`agent-skills.instructions.md`)
- 应用范围: `.github/skills/**/SKILL.md` 和 `.claude/skills/**/SKILL.md`
- 功能: 指导创建高质量 Agent Skills
- 使用场景: 创建或修改技能文件时

## 如何使用

1. **codebuddy 会自动读取这些 instructions**
   - 每次对话开始时
   - 根据 `applyTo` 字段匹配相关文件

2. **Instructions 的工作原理**
   - Frontmatter 定义应用范围
   - 主体内容提供具体指导
   - 按匹配顺序加载多个指令

3. **添加新 Instructions**
   - 在 `.github/instructions/` 创建新的 `.md` 文件
   - 包含 frontmatter: `applyTo: '<glob-pattern>'`
   - 编写具体的指导内容

## 示例 Instruction 格式

```markdown
---
applyTo: '**/src/**/*.ts'
description: 我的自定义指令
---

# 指令标题

具体的指导内容...
```

## 注意事项

- Instructions 文件使用 glob 模式匹配文件
- 多个 instruction 可能同时应用
- 保持指令简洁明确
- 定期更新以反映项目变化
