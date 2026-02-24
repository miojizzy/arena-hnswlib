# 活跃上下文

### 当前工作重点
- 距离计算 SIMD 加速已完成（L2 + IP 均支持 AVX）
- 所有核心算法已实现并测试通过
- 重点转向进一步优化（FMA、SSE 扩展）和基准测试

### 最近变更
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
- 分析基准测试结果以识别优化机会
- 考虑在基准测试中实现内存使用追踪
- 根据基准测试洞察进一步优化HNSW邻居更新和搜索逻辑
- 为HNSW添加更多边界情况和压力测试
- 改进文档和代码注释，特别是hnswalg.h
- 考虑添加精确率/召回率准确性基准测试

### 当前决策与考虑
- 在DataStore和HNSW中使用std::unique_ptr进行内存管理（与SpacePtr一致）
- 使用static_assert进行模板约束
- 使用静态链接实现距离函数
- 单元测试是正确性的主要验证方法
- 基准测试使用固定随机种子（42）以确保可复现性
- 基准测试仅使用L2距离（根据TASK003要求）
- 默认基准测试参数：DIM=128, DATA_SIZE=10000, QUERY_SIZE=100, K=10
