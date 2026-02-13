## 系统架构
- Header-only设计：所有核心算法在include/arena-hnswlib/下实现
- 主要组件：数据存储（data_store.h）、暴力搜索（bruteforce.h）、HNSW算法核心（arena_hnswlib.h）、距离度量（math_utils.h、space_ip.h、space_l2.h）

## 关键技术决策
- C++17标准，兼容主流编译器
- CMake构建系统，支持GoogleTest/Google Benchmark
- 单元测试和基准测试分离

## 设计模式
- 接口/实现分离
- 优先使用组合而非继承
- 使用模板支持泛型数据类型

## 组件关系
- arena_hnswlib.h依赖data_store.h和math_utils.h
- 测试和基准测试链接到header-only库

## 目录结构
- include：库头文件
    - arena-hnswlib：hnswlib的优化版本
- src：库源代码（当前为空，header-only）
- test：测试用例
    - utest：单元测试
- script：构建、测试和基准测试脚本，从项目根目录运行
