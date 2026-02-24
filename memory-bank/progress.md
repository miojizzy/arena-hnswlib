# 进度

### 已完成功能
- BruteForceSearch和DataStore类已实现并测试
- InnerProductSpace和L2Space距离度量已实现并测试
- L2（欧几里得）距离支持，使用平方L2以提高效率，AVX SIMD 加速
- IP（内积）距离支持 AVX SIMD 加速（`IPSqrSIMDAVX<float/double>`，残差路径，USE_AVX 门控）
- HNSW（HierarchicalNSW）类：核心结构和大部分方法已实现
- 所有核心组件的单元测试（BruteForceSearch、DataStore、HierarchicalNSW、距离度量）
- BruteForce和HNSW索引的综合基准测试（构建时间、查询时间、可扩展性）
- 随机向量生成，支持可设置种子的RNG和可选L2归一化
- AlignedVector对齐向量容器，支持SIMD优化的内存对齐存储
  - AlignedVector：拥有内存的move-only容器
  - AlignedVectorView：非拥有的可拷贝视图
  - DataStoreAligned已集成，使用AlignedVector作为底层存储

### 待完成功能
- 进一步优化和测试HNSW索引逻辑（邻居选择、层级分配、搜索）
- 为HNSW添加更全面和边界情况的单元测试
- 文档化所有公共API并阐明使用模式
- 考虑添加内存使用基准测试
- 根据基准测试结果评估性能优化

### 当前状态
- BruteForceSearch和DataStore：稳定
- InnerProductSpace和L2Space：稳定
- HNSW：核心逻辑已实现，基础和边界情况测试已存在，部分高级功能可能不完整
- 基准测试：综合基准测试套件已实现并正常工作
- AlignedVector：已实现并测试，支持32/64字节对齐，已集成到DataStoreAligned

### 已知问题
- HNSW：邻居更新逻辑和搜索启发式可能需要进一步验证
- HNSW中部分方法较复杂，可增加更多注释
- HNSW构建时间对于大数据集较长（预期行为）
- DEBUG模式构建的基准测试显示性能警告
