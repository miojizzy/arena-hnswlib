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

**目录模块说明**
- include 存放库的头文件
    - arena-hnswlib 为hnswlib的优化版本 
- src 存放库源码（由于当前仅有头文件，暂时空）
- test 存放测试用例
    - utest 存放单元测试
- script 存放脚本，包括编译、测试、性能测试等，在项目根目录执行脚本