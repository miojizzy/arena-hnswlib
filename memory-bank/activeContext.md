# 活跃上下文

### 当前工作重点
- 多线程并发安全：VisitedTable 已改为请求粒度局部变量，searchKnn 线程安全
- VisitedTable 进一步优化为 bitmap（uint64_t），内存减少 8x，cache 更友好
- 所有核心算法已实现并测试通过，9/9 单元测试绿色

### 最近变更
- 完成 VisitedTable bitmap 优化（2026-02-25）
  - `VisitedTable` 底层从 `vector<uint8_t>` 改为 `vector<uint64_t>` bitmap
  - `mark()` / `isVisited()` 使用位运算，内存占用 8x 更小
  - 删除已无调用方的 `reset()` 方法及 `dirty_` 成员
  - HNSW QuerySingle -2.4%，QueryBatch -3.0%，VaryingDataSize/1000 -2.3%

- 完成 VisitedTable 请求粒度重构（TASK010 - 2026-02-25）
  - 删除 `HierarchicalNSW` 共享成员 `mutable VisitedTable visited_table_`
  - `updateNewPointAtLevel` 和 `searchKnnAtBaseLevel` 各自创建局部 `VisitedTable`
  - 新增 `tests/unit/test_concurrent.cpp`，3 个并发测试（BruteForce 8 线程并发搜索、HNSW 4 线程独立构建、HNSW 8 线程真正并发搜索）
  - benchmark 无性能回退，QuerySingle 持平或略有提升

- 完成 IP 距离 AVX SIMD 加速（TASK009 - 2026-02-24）
  - `space_ip.h` 新增 `IPSqrSIMDAVX<float/double>`、`IPSqrSIMD16ExtResiduals`、`InnerProductSIMD`
  - `InnerProductDistance` 更新为经由 `InnerProductSIMD` 调用，自动走 SIMD 路径
  - `test_space_ip.cpp` 从 3 个测试扩展到 21 个，覆盖 aligned/unaligned/small/large/double 各路径，全部通过

- 完成基准测试套件实现（TASK003 - 2026-02-06）
  - 创建benchmark_index.cpp，包含VectorGenerator类
  - 实现可设置种子随机向量生成，支持L2归一化选项
  - 添加BruteForce和HNSW索引的基准测试固件
  - 基准测试构建时间、单次查询和批量查询性能
  - 添加可变K值和数据大小的参数化基准测试
  - 在benchmarks/README.md中创建完整文档
  - 修复编译问题：添加<stdexcept>头文件，修正指针用法
  - 更新CMakeLists.txt和run_benchmarks.sh脚本
- 完成L2（欧几里得）距离支持（TASK002 - 2026-02-06）
  - 在math_utils.h中实现L2DistanceSquared函数
  - 在space_l2.h中创建L2Space类
  - 添加完整的单元测试（test_math_utils.cpp和test_space_l2.cpp）
  - 所有测试通过
- 添加并稳定BruteForceSearch和DataStoreAligned类
- 实现InnerProductSpace和L2Space距离函数
- HNSW（HierarchicalNSW）类：核心逻辑已实现，包括邻居选择、层级分配和搜索
- 添加HNSW的完整单元测试（初始化、添加/搜索、最大元素数）

### 下一步计划
- 考虑添加精确率/召回率准确性基准测试
- 考虑 FMA 指令进一步加速距离计算
- `updateExistPointAtLevel` 中的 `vector<bool> local_seen(elementSize_)` 可换成 VisitedTable bitmap 统一风格

### 当前决策与考虑
- `VisitedTable` 使用 bitmap（`vector<uint64_t>`），请求粒度局部创建，线程安全
- `searchKnn` 为只读操作，多线程并发搜索安全（VisitedTable 已无共享状态）
- 在DataStore和HNSW中使用std::unique_ptr进行内存管理（与SpacePtr一致）
- 使用静态链接实现距离函数，SpaceT 作模板参数确保内联
- 单元测试是正确性的主要验证方法，benchmark_release.txt 记录最新性能基线
- 基准测试使用固定随机种子（42）以确保可复现性
