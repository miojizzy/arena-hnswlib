# TASK004 - 实现对齐向量容器 AlignedVector

**状态：** 已完成
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

### 最终设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 类设计 | 两个独立类 | `AlignedVector`（拥有）和 `AlignedVectorView`（非拥有）分离，职责清晰 |
| 拷贝构造 | AlignedVector禁用，View允许 | Owner 避免意外深拷贝和 double-free；View 是引用语义，允许拷贝 |
| 深拷贝 | 不提供 | 简化设计，如需要可手动 memcpy |
| 动态扩容 | 不支持 | 简化设计，符合 SIMD 场景（固定维度） |
| 迭代器 | 不支持 | 只提供 `[]` 随机访问，简化接口 |
| 对齐值 | 模板参数，编译期固定 | 默认32字节，支持32/64字节对齐 |

### 内存模式

| 类 | owns_memory | 析构行为 | 可修改数据 | 可拷贝 |
|----|-------------|----------|-----------|--------|
| AlignedVector | ✓ | 释放内存 | ✓ | ✗ (仅移动) |
| AlignedVectorView | ✗ | 不释放 | ✓ | ✓ |

### 对齐容量计算

当需要在一个大块对齐内存中存储多个向量时，每个向量的容量按对齐边界向上取整：

```cpp
// 计算 count 个元素需要的对齐容量
size_type calc_aligned_capacity(size_type count) {
    const size_type bytes = count * sizeof(T);
    const size_type aligned_bytes = ((bytes + Alignment - 1) / Alignment) * Alignment;
    return aligned_bytes / sizeof(T);
}
```

示例：64 字节对齐，2 维 float 向量（8 字节）
```
无 padding：  vec0@0x00 ✓  vec1@0x08 ✗  vec2@0x10 ✗
有 padding：  vec0@0x00 ✓  vec1@0x40 ✓  vec2@0x80 ✓
```

## 实现内容

### 文件
- `include/arena-hnswlib/aligned_vector.h` - 主要实现

### AlignedVector 类
- **构造函数**：`explicit AlignedVector(size_type count)`
- **移动语义**：移动构造/赋值，禁用拷贝
- **访问器**：`data()`, `size()`, `capacity()`, `alignment()`
- **限制**：只支持 trivially copyable 类型

### AlignedVectorView 类
- **构造函数**：`AlignedVectorView(pointer ptr, size_type count)`
- **拷贝语义**：默认拷贝构造/赋值（引用语义）
- **访问器**：`data()`, `size()`, `capacity()`, `alignment()`
- **限制**：只支持 trivially copyable 类型

### 集成到 DataStoreAligned
`DataStoreAligned` 已更新为使用 `AlignedVector` 作为底层存储：
- 内部使用 `AlignedVector<T, Alignment>` 管理整个对齐内存
- 新增 `getView(id)` 方法返回 `AlignedVectorView`
- 新增 `getVector()` 方法访问底层 `AlignedVector`

## 进度追踪

**整体状态：** 已完成 - 100%

### 子任务
| ID | 描述 | 状态 | 更新日期 | 备注 |
|----|------|------|----------|------|
| 1.1 | 实现 AlignedVector 类 | 已完成 | 2026-02-14 | 拥有内存，move-only |
| 1.2 | 实现 AlignedVectorView 类 | 已完成 | 2026-02-14 | 非拥有视图，可拷贝 |
| 1.3 | 集成到 DataStoreAligned | 已完成 | 2026-02-14 | 使用 AlignedVector 作为底层存储 |
| 1.4 | 编写单元测试 | 已完成 | 2026-02-14 | 15个测试用例全部通过 |

## 进度日志
### 2026-02-14
- 完成 AlignedVector 设计讨论
- 初始实现包含完整 STL 风格接口（迭代器等）
- 根据用户反馈简化设计：
  - 移除迭代器支持，只保留 `[]` 随机访问
  - 固定大小，不支持动态扩容
  - 拆分为 AlignedVector 和 AlignedVectorView 两个类
- 修复用户代码中的 bug（成员初始化顺序、const 修饰符等）
- 更新测试用例匹配最终接口
- 将 DataStoreAligned 改为使用 AlignedVector 作为底层存储
- 所有 84 个测试通过
