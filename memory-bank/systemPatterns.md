**系统架构**：
- 采用 header-only 设计，核心算法均在 include/arena-hnswlib/ 下实现
- 主要组件：数据存储（data_store.h）、暴力检索（bruteforce.h）、HNSW 算法主体（arena_hnswlib.h）、距离度量（math_utils.h, space_ip.h）

**关键技术决策**：
- C++17 标准，兼容主流编译器
- CMake 构建，支持 GoogleTest/Google Benchmark
- 单元测试与基准测试分离

**设计模式**：
- 接口/实现分离
- 组合优于继承
- 以模板泛型支持多种数据类型

**组件关系**：
- arena_hnswlib.h 依赖 data_store.h、math_utils.h
- 测试与 benchmark 通过链接 header-only 库进行
# System Patterns

- c++17

## System Architecture
[Describe the overall system architecture.]

## Key Technical Decisions
- [List the key technical decisions made for the arena hnswlib.]

## Design Patterns
- [List the design patterns used in the arena hnswlib.]

## Component Relationships
[Describe the relationships between different components in the system.]