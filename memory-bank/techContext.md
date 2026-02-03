**主要技术栈**：
- C++17
- CMake 3.15+
- GoogleTest（单元测试）
- Google Benchmark（性能基准）

**开发环境**：
- Ubuntu 24.04.3 LTS
- 推荐使用 VSCode + Dev Container

**技术约束**：
- 仅依赖标准库与 GoogleTest/Benchmark
- 需支持主流 Linux 平台

**依赖管理**：
- 通过 CMake FetchContent 拉取 GoogleTest/Benchmark