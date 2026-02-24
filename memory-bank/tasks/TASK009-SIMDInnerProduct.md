# [TASK009] - SIMD Inner Product Distance 加速

**Status:** Completed  
**Added:** 2026-02-24  
**Updated:** 2026-02-24

## Original Request
参考 L2 距离的 SIMD 实现，为 IP（Inner Product）距离计算添加 AVX SIMD 加速支持。

## Thought Process

### 现有 L2 SIMD 实现分析

`space_l2.h` 中的 L2 SIMD 实现结构如下：

```
L2Squared()                  — 纯标量实现，用于残差或无 AVX 时的回退
L2SqrSIMDAVX<float>()        — AVX 特化，每轮处理 16 floats（2 × 8）
L2SqrSIMDAVX<double>()       — AVX 特化，每轮处理 8 doubles（2 × 4）
L2SqrSIMD16ExtResiduals()    — 对齐到 16 float 边界，尾部用标量补完
L2SquaredDistance()          — 统一入口：USE_AVX 决定是否走 SIMD 路径
```

计算逻辑（float AVX 为例）：
```
diff = v1 - v2
sum += diff * diff      (展开 2 次，每次处理 8 floats)
横向求和 TmpRes[0..7]
```

### IP SIMD 实现设计

Inner Product 计算 `sum(a[i] * b[i])`，比 L2 更简单——去掉减法，直接乘加。
结构与 L2 完全对称，只需将"差的平方"改为"乘积累加"：

#### 1. 标量基线：`InnerProduct<dist_t>`（已实现，保持不变）
```cpp
template<typename dist_t>
inline static dist_t InnerProduct(const dist_t* a, const dist_t* b, size_t dim);
```

#### 2. AVX 核心：`IPSqrSIMDAVX<float>` 与 `IPSqrSIMDAVX<double>`

对应 L2 的 `L2SqrSIMDAVX`，前向声明 + 全特化：

```cpp
// 前向声明
#if defined(USE_AVX)
template<typename dist_t>
dist_t IPSqrSIMDAVX(const dist_t* pVect1, const dist_t* pVect2, size_t dim);

// float 特化：每轮处理 16 floats (2 × 8)
template<>
float IPSqrSIMDAVX<float>(...) {
    float PORTABLE_ALIGN64 TmpRes[8];
    const float* pEnd1 = pVect1 + (dim >> 4 << 4);  // 对齐到 16
    __m256 sum = _mm256_set1_ps(0);
    while (pVect1 < pEnd1) {
        v1 = _mm256_loadu_ps(pVect1); pVect1 += 8;
        v2 = _mm256_loadu_ps(pVect2); pVect2 += 8;
        sum = _mm256_add_ps(sum, _mm256_mul_ps(v1, v2));   // ← 变化点

        v1 = _mm256_loadu_ps(pVect1); pVect1 += 8;
        v2 = _mm256_loadu_ps(pVect2); pVect2 += 8;
        sum = _mm256_add_ps(sum, _mm256_mul_ps(v1, v2));
    }
    _mm256_store_ps(TmpRes, sum);
    return TmpRes[0]+TmpRes[1]+TmpRes[2]+TmpRes[3]+TmpRes[4]+TmpRes[5]+TmpRes[6]+TmpRes[7];
}

// double 特化：每轮处理 8 doubles (2 × 4)
template<>
double IPSqrSIMDAVX<double>(...) {
    // 类似 float 版本，使用 __m256d / _mm256_loadu_pd / _mm256_mul_pd / _mm256_add_pd
}
#endif
```

**优化机会**：若目标支持 FMA（`__FMA__`），乘加步骤可改用 `_mm256_fmadd_ps` 减少指令数和舍入误差：
```cpp
sum = _mm256_fmadd_ps(v1, v2, sum);   // sum += v1 * v2，1 条指令替代 mul+add
```
可独立于 USE_AVX 以 `defined(__FMA__)` 门控，保持向后兼容。

#### 3. 残差处理：`IPSqrSIMD16ExtResiduals<dist_t>`

对应 L2 的 `L2SqrSIMD16ExtResiduals`，处理 `dim % 16 != 0` 的尾部：

```cpp
#if defined(USE_AVX)
template<typename dist_t>
static dist_t IPSqrSIMD16ExtResiduals(const dist_t* pVect1, const dist_t* pVect2, size_t dim) {
    size_t dim16 = dim >> 4 << 4;
    dist_t res = IPSqrSIMDAVX<dist_t>(pVect1, pVect2, dim16);
    dist_t res_tail = InnerProduct<dist_t>(pVect1 + dim16, pVect2 + dim16, dim - dim16);
    return res + res_tail;
}
#endif
```

#### 4. 统一入口：`InnerProductSIMD<dist_t>`

替换原来的 `InnerProduct<dist_t>`，作为 `InnerProductDistance` 的内部调用：

```cpp
template<typename dist_t>
inline static dist_t InnerProductSIMD(const dist_t* a, const dist_t* b, size_t dim) {
#if defined(USE_AVX)
    if (dim % 16 == 0)
        return IPSqrSIMDAVX<dist_t>(a, b, dim);
    else
        return IPSqrSIMD16ExtResiduals<dist_t>(a, b, dim);
#else
    return InnerProduct<dist_t>(a, b, dim);
#endif
}
```

#### 5. 更新距离入口函数

```cpp
template<typename dist_t>
inline static dist_t InnerProductDistance(const dist_t* a, const dist_t* b, size_t dim) {
    return 1.0f - InnerProductSIMD<dist_t>(a, b, dim);
}
```

`InnerProductSpace::distFunc` 保持不变，它调用 `InnerProductDistance` 即自动获得 SIMD 加速。

### 与 L2 实现的对称性总结

| 层次 | L2 | IP（新增） |
|------|----|----|
| 标量基线 | `L2Squared` | `InnerProduct`（已有） |
| AVX 核心 float | `L2SqrSIMDAVX<float>` | `IPSqrSIMDAVX<float>` |
| AVX 核心 double | `L2SqrSIMDAVX<double>` | `IPSqrSIMDAVX<double>` |
| 残差处理 | `L2SqrSIMD16ExtResiduals` | `IPSqrSIMD16ExtResiduals` |
| 统一入口 | `L2SquaredDistance` | `InnerProductSIMD` |
| 距离函数 | `L2SquaredDistance`（直接） | `InnerProductDistance`（包装 SIMD） |

### 关键差异（L2 vs IP）

- L2：计算 `diff = v1 - v2; sum += diff * diff` → 需要 sub + mul
- IP：计算 `sum += v1 * v2` → 只需 mul（更少指令，可用 FMA 合并）
- IP 的 SIMD 实现理论上比 L2 更快（少一次向量减法）

### 测试策略

1. **正确性**：在 `test_space_ip.cpp` 中添加验证：
   - 随机向量的标量结果与 SIMD 结果误差在 `1e-5` 以内
   - 测试 `dim % 16 == 0` 与 `dim % 16 != 0` 两种路径
   - 测试 float 和 double 两种类型

2. **性能**：在 `benchmark_math_utils.cpp` 中添加基准测试对比标量 vs AVX。

## Implementation Plan

- [x] 1. 在 `space_ip.h` 中添加 AVX 前向声明（仿照 `space_l2.h`）
- [x] 2. 实现 `IPSqrSIMDAVX<float>` 全特化
- [x] 3. 实现 `IPSqrSIMDAVX<double>` 全特化
- [x] 4. 实现 `IPSqrSIMD16ExtResiduals<dist_t>`
- [x] 5. 实现 `InnerProductSIMD<dist_t>` 统一入口
- [x] 6. 更新 `InnerProductDistance` 调用 `InnerProductSIMD`
- [ ] 7. （可选）添加 FMA 优化路径（`__FMA__` 门控）— 留待后续
- [x] 8. 在 `test_space_ip.cpp` 中添加 SIMD 正确性测试（21 个用例全部通过）
- [ ] 9. 在 `benchmark_math_utils.cpp` 中添加性能对比基准 — 留待后续

## Progress Tracking

**Overall Status:** Completed - 90%（FMA 与 benchmark 留待后续）

### Subtasks
| ID | Description | Status | Updated | Notes |
|----|-------------|--------|---------|-------|
| 9.1 | AVX 前向声明 | Complete | 2026-02-24 | `#if defined(USE_AVX)` 门控 |
| 9.2 | `IPSqrSIMDAVX<float>` 实现 | Complete | 2026-02-24 | `_mm256_loadu_ps` + `_mm256_mul_ps` |
| 9.3 | `IPSqrSIMDAVX<double>` 实现 | Complete | 2026-02-24 | `_mm256_loadu_pd` + `_mm256_mul_pd` |
| 9.4 | `IPSqrSIMD16ExtResiduals` 实现 | Complete | 2026-02-24 | 尾部用标量 `InnerProduct` 补完 |
| 9.5 | `InnerProductSIMD` 统一入口 | Complete | 2026-02-24 | dim%16 分派 |
| 9.6 | 更新 `InnerProductDistance` | Complete | 2026-02-24 | 改调 `InnerProductSIMD` |
| 9.7 | FMA 可选优化 | Not Started | 2026-02-24 | `__FMA__` 门控，留待后续 |
| 9.8 | 单元测试（正确性） | Complete | 2026-02-24 | 21 个用例全部通过，覆盖 aligned/unaligned/small/large |
| 9.9 | 基准测试（性能）| Not Started | 2026-02-24 | 标量 vs AVX 吞吐量对比，留待后续 |

## Progress Log
### 2026-02-24
- 创建任务文件
- 分析 L2 SIMD 实现结构
- 完成完整的 IP SIMD 设计方案
- 实现 `space_ip.h` 中的完整 AVX SIMD 支持：
  - `IPSqrSIMDAVX<float>` / `IPSqrSIMDAVX<double>` AVX 核心
  - `IPSqrSIMD16ExtResiduals` 残差处理
  - `InnerProductSIMD` 统一入口（`USE_AVX` 门控）
  - `InnerProductDistance` 更新为调用 `InnerProductSIMD`
- 完善 `test_space_ip.cpp`：从 3 个测试扩展到 21 个，全部通过
  - 新增 `InnerProductDistanceTest`（3个）：基本、正交距离、对称性
  - 新增 `InnerProductSIMDTest`（6个）：aligned/unaligned/small/double/large 各路径
  - 扩展 `InnerProductSpaceTest`（5个）：dim+dataSize、distFunc非空、correctness、静态方法一致性、对称性
- 任务标记为完成（FMA 和 benchmark 子任务留待后续）
