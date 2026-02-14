# TASK004 - 实现对齐向量容器 AlignedVector

**状态：** 待处理
**添加日期：** 2026-02-14
**更新日期：** 2026-02-14

## 原始需求
实现一个对齐向量容器，满足以下要求：
1. 底层数据保证 32/64 字节对齐，便于 SIMD 操作
2. 作为其他容器元素时（如 `std::vector<AlignedVector>`），底层数据仍保持对齐
3. 支持两种内存模式：
   - Owner 模式：自己申请对齐内存，析构时释放
   - View 模式：指向外部已对齐的内存，不拥有、不释放
4. 提供计算对齐偏移量的工具接口

## 思考过程

### 问题背景
在修复 `_mm256_load_ps` 导致的段错误时，发现 `std::vector<float>` 不保证 32 字节对齐。需要实现对齐容器来避免此类问题。

### 设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 拷贝构造 | 禁用 | 避免意外深拷贝和 double-free |
| 深拷贝 | `clone()` 方法 | 显式表达意图 |
| 动态扩容 | 不支持 | 简化设计，符合 SIMD 场景（固定维度） |
| View 修改 | 允许改值，不可扩容 | 满足数据修改需求 |
| 只读视图 | 增加 `AlignedVectorConstView` | 支持只读场景 |
| 对齐值 | 模板参数，编译期固定 | 参考项目现有 `PORTABLE_ALIGN32/64` |
| 对齐检查 | 运行时断言 | 调试模式下验证外部内存对齐 |

### 内存模式

| 模式 | owns_memory_ | 析构行为 | 可修改数据 | 可扩容 |
|------|-------------|----------|-----------|--------|
| Owner | true | 释放内存 | ✓ | ✗ |
| View | false | 不释放 | ✓ | ✗ |
| ConstView | false | 不释放 | ✗ | ✗ |

### 对齐偏移问题

当在一个大块对齐内存中存储多个向量时，只有第一个向量地址对齐。需要 padding 保证每个向量起始地址都对齐。

示例：64 字节对齐，2 维 float 向量（8 字节）
```
无 padding：  vec0@0x00 ✓  vec1@0x08 ✗  vec2@0x10 ✗
有 padding：  vec0@0x00 ✓  vec1@0x40 ✓  vec2@0x80 ✓
```

## 实现计划
- 创建 `include/arena-hnswlib/aligned_vector.h` 文件
- 实现 `AlignedVector<T, Alignment>` 类
  - 构造函数：默认、指定大小、大小+填充值
  - 工厂方法：`view()`, `take()`
  - 移动语义，禁用拷贝，提供 `clone()`
  - 访问器：`data()`, `size()`, `capacity()`, `operator[]`, `at()`, `front()`, `back()`
  - 迭代器：`begin()`, `end()`, `cbegin()`, `cend()`
  - 修改器：`resize()`, `clear()`（仅 Owner 模式）
  - 静态工具：`aligned_stride()`, `offset_at()`, `total_capacity()`, `pointer_at()`
- 实现 `AlignedVectorConstView<T, Alignment>` 类
  - 只读视图，不允许修改数据
- 添加单元测试

## 进度追踪

**整体状态：** 未开始 - 0%

### 子任务
| ID | 描述 | 状态 | 更新日期 | 备注 |
|----|------|------|----------|------|
| 1.1 | 实现 AlignedVector 核心类 | 未开始 | - | - |
| 1.2 | 实现 AlignedVectorConstView 类 | 未开始 | - | - |
| 1.3 | 添加静态对齐工具方法 | 未开始 | - | - |
| 1.4 | 编写单元测试 | 未开始 | - | - |

## 进度日志
### 2026-02-14
- 完成 AlignedVector 设计讨论
- 确定实现方案和接口设计
