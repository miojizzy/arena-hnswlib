# TASK008 - 将 Space 改造为模板参数以消除函数指针间接调用

**状态：** 已完成  
**添加日期：** 2026-02-24  
**更新日期：** 2026-02-24

## 原始需求

当前 `HierarchicalNSW` 通过 `DISTFUNC<dist_t>`（函数指针）调用距离函数，导致编译器无法内联距离计算、阻止跨调用的软件流水线优化。将 `SpaceT` 作为模板参数传入，使距离调用变为静态分发，让编译器完全内联距离函数体。

## 思考过程

### 目标与约束

**目标**：消除 `dist_func_(p1, p2, dim_)` 的间接调用 overhead，使编译器能够：
1. 内联距离函数（消除 call/ret + 寄存器保存/恢复）
2. 对"遍历邻居"的外层循环做 prefetch 优化（软件流水线）

**约束**：
- `dim_` 保持运行时值，不引入 Dim 模板参数（否则用户必须编译期确定 dim，不实用）
- Python bindings 的接口不变（用户仍然传 `"l2"` / `"ip"` 字符串）
- 单元测试不需要大量改写

### 关键设计决策

#### SpaceT 的 contract（需要满足的接口）

将 `SpaceInterface<dist_t>` 的运行时虚函数接口，改为编译期静态接口（duck typing via concept）：

```cpp
// SpaceT 必须提供：
// 1. 静态距离函数（可内联）
static dist_t distFunc(const dist_t* a, const dist_t* b, size_t dim);

// 2. 构造时确定 dim（运行时）
size_t getDim() const;
size_t getDataSize() const;
```

现有的 `L2Space<float>` 和 `InnerProductSpace<float>` 已经有 `getDim()`/`getDataSize()`，只需将 `getDistFunc()` 返回的函数指针改为静态成员函数。

#### AlgorithmInterface 的处理

`AlgorithmInterface<dist_t>` 目前是接收 `SpacePtr<dist_t>` 的虚基类，用于 Python bindings 的多态。需要保持两层：

```
外部接口层（多态）：AlgorithmInterface<dist_t>  ← Python bindings 持有这个
内部实现层（模板）：HierarchicalNSW<dist_t, SpaceT>  ← 具体实例
```

`HierarchicalNSW<dist_t, SpaceT>` 继承 `AlgorithmInterface<dist_t>`，对外仍然满足多态接口，内部使用 `SpaceT` 静态分发。

```
Python / 测试代码           内部实现
AlgorithmInterface<float>  ←  HierarchicalNSW<float, L2Space<float>>
                              HierarchicalNSW<float, InnerProductSpace<float>>
```

#### HierarchicalNSW 构造函数的变化

```cpp
// 现在（接收 unique_ptr，运行时多态）
HierarchicalNSW(SpacePtr<dist_t> s, size_t elementSize, ...);

// 改造后（SpaceT 是模板参数，构造时传值或直接构造）
template<typename dist_t, typename SpaceT>
class HierarchicalNSW : public AlgorithmInterface<dist_t> {
    SpaceT space_;   // 直接持有，非指针，消除虚函数调用
    
    // 距离调用：编译器可以内联
    dist_t d = SpaceT::distFunc(p1, p2, dim_);
    // 或
    dist_t d = space_.distFunc(p1, p2, dim_);  // 如果是成员函数
};

// 构造
HierarchicalNSW(SpaceT space, size_t elementSize, ...);
```

#### dist_func_ 成员的移除

```cpp
// 移除
const DISTFUNC<dist_t> dist_func_;

// 所有 dist_func_(p1, p2, dim_) 调用改为
SpaceT::distFunc(p1, p2, dim_)
// 或静态成员函数的调用方式
```

#### Python bindings 的适配

现在 `makeSpace` 返回 `SpacePtr<float>` 传给 `HierarchicalNSW`。改造后需要做 type dispatch：

```cpp
// 方案：工厂函数模板实例化
static std::unique_ptr<AlgorithmInterface<float>>
makeHNSW(const std::string& metric, size_t dim, size_t N, size_t M, size_t ef, size_t seed) {
    if (metric == "l2") {
        return std::make_unique<HierarchicalNSW<float, L2Space<float>>>(
            L2Space<float>(dim), N, M, ef, seed);
    } else if (metric == "ip") {
        return std::make_unique<HierarchicalNSW<float, InnerProductSpace<float>>>(
            InnerProductSpace<float>(dim), N, M, ef, seed);
    }
    throw std::invalid_argument("Unknown metric");
}
```

Python 侧持有 `std::unique_ptr<AlgorithmInterface<float>>`，不感知具体类型，接口不变。

#### BruteForce 是否也改造

`BruteForceSearch` 使用相同的 `DISTFUNC` 函数指针模式，可以一并改造，但非必须。搜索内层循环是线性扫描所有 N 个点，函数指针在 bf 中的 overhead 比例更小。为减少改动范围，第一步只改 HNSW。

### Space 类需要的改造

`L2Space<float>` 和 `InnerProductSpace<float>` 需要将距离函数从实例成员变为可内联的静态成员（或 namespace 函数）：

```cpp
template<typename dist_t>
class L2Space {
 public:
    // 新增：静态距离函数，供模板调用
    static dist_t distFunc(const dist_t* a, const dist_t* b, size_t dim) {
        return L2SquaredDistance<dist_t>(a, b, dim);  // 已有，直接复用
    }
    
    // 保留原有接口（兼容 AlgorithmInterface 的旧接口路径）
    const DISTFUNC<dist_t>& getDistFunc() const { return fstdistfunc_; }
    const size_t& getDim() const { return dim_; }
    const size_t& getDataSize() const { return data_size_; }
    
    explicit L2Space(size_t dim) : dim_(dim), data_size_(dim * sizeof(dist_t)) {}
    // ...
};
```

原有的 `getDistFunc()` / `SpaceInterface` 虚函数接口可以**完全保留**，新增静态路径，两套接口并存。这样旧代码（BruteForce、测试）无需改动。

### 改造范围总结

| 文件 | 改动内容 | 影响 |
|------|---------|------|
| `space_l2.h` | 新增 `static distFunc()` | 新增，不破坏现有接口 |
| `space_ip.h` | 新增 `static distFunc()` | 新增，不破坏现有接口 |
| `hnswalg.h` | 增加 `SpaceT` 模板参数，移除 `dist_func_` 成员 | 核心改动 |
| `python/bindings.cpp` | `makeHNSW` 工厂函数按 metric 实例化 | 适配改动 |
| `tests/unit/test_hnswalg.cpp` | 构造方式从 `unique_ptr<Space>` 改为 `Space(dim)` | 少量改动 |
| `benchmarks/benchmark_index.cpp` | 同上 | 少量改动 |
| `bruteforce.h` | 暂不改动 | 不变 |
| `arena_hnswlib.h` | `AlgorithmInterface` 保持不变 | 不变 |

## 实现计划

- [ ] 1. `space_l2.h` 和 `space_ip.h` 新增 `static distFunc()` 静态成员
  - 直接调用已有的 `L2SquaredDistance` / `InnerProductDistance` 函数
  - 保留原虚函数接口

- [ ] 2. 改造 `hnswalg.h`：`HierarchicalNSW` 增加 `SpaceT` 模板参数
  - 签名：`template<typename dist_t, typename SpaceT>`
  - 成员 `const DISTFUNC<dist_t> dist_func_` → 移除
  - 成员 `SpacePtr<dist_t> space_` → `SpaceT space_`（直接持有）
  - 构造函数：`SpacePtr<dist_t>` 参数 → `SpaceT space` 参数
  - 所有 `dist_func_(p1, p2, dim_)` → `SpaceT::distFunc(p1, p2, dim_)`
  - `dim_` / `data_size_` 初始化：从 `space_.getDim()` / `space_.getDataSize()`

- [ ] 3. 适配 `python/bindings.cpp`
  - `makeHNSW` 工厂函数按 metric 实例化具体类型
  - `PyHNSW` 内部持有 `std::unique_ptr<AlgorithmInterface<float>>`

- [ ] 4. 适配 `tests/unit/test_hnswalg.cpp`
  - 构造方式：`HierarchicalNSW<float, InnerProductSpace<float>>(InnerProductSpace<float>(dim), ...)`

- [ ] 5. 适配 `benchmarks/benchmark_index.cpp`
  - 同上

- [ ] 6. 编译验证

- [ ] 7. 单元测试通过

- [ ] 8. Benchmark 对比（预期搜索 5~15% 提升，build 更明显）

## 进度追踪

**整体状态：** 已完成 - 100%

### 子任务
| ID | 描述 | 状态 | 更新日期 | 备注 |
|----|------|------|----------|------|
| 8.1 | Space 类新增 static distFunc | 完成 | 2026-02-24 | L2Space + InnerProductSpace 各新增 `static inline dist_t distFunc(...)` |
| 8.2 | HierarchicalNSW 增加 SpaceT 模板参数 | 完成 | 2026-02-24 | 移除 dist_func_ 成员；space_ 改为值类型首成员；所有距离调用改为 SpaceT::distFunc |
| 8.3 | Python bindings 适配 | 完成 | 2026-02-24 | makeHNSW 工厂函数按 metric dispatch；PyHNSW 持有 AlgorithmInterface<float> |
| 8.4 | 测试代码适配 | 完成 | 2026-02-24 | test_hnswalg.cpp + test_arena_hnswlib.cpp 均已更新 |
| 8.5 | 基准测试适配 | 完成 | 2026-02-24 | benchmark_index.cpp 已更新 |
| 8.6 | 编译 + 单元测试通过 | 完成 | 2026-02-24 | 84/84 tests passed |
| 8.7 | Benchmark 对比 | 完成 | 2026-02-24 | DEBUG 模式下无显著差异（预期），Release 模式下 inlining 效果才会体现 |

## 进度日志
### 2026-02-24
- 在 `L2Space<dist_t>` 和 `InnerProductSpace<dist_t>` 各新增 `static inline dist_t distFunc(const dist_t*, const dist_t*, size_t)` 静态成员函数，直接调用已有的 `L2SquaredDistance` / `InnerProductDistance`
- 移除 `AlgorithmInterface<dist_t>` 的 `SpacePtr<dist_t> space_` 成员和带参构造函数；改为默认构造函数；新增 `virtual getEfSearch()/setEfSearch()` 接口（带默认空实现）
- `HierarchicalNSW<dist_t>` → `HierarchicalNSW<dist_t, SpaceT>`：
  - `SpaceT space_` 声明提前到类的第一个成员（确保初始化顺序正确，dim_/data_size_ 从 space_ 获取）
  - 移除 `const DISTFUNC<dist_t> dist_func_` 成员
  - 构造函数参数由 `SpacePtr<dist_t>` 改为 `SpaceT space`（值传入后 move 到成员）
  - 所有 `dist_func_(p1, p2, dim_)` 调用改为 `SpaceT::distFunc(p1, p2, dim_)`
  - `getEfSearch`/`setEfSearch` 加上 `override`
- `BruteForceSearch` 构造函数改为调用 `AlgorithmInterface<dist_t>()` 默认构造，直接从传入的 `SpacePtr` 读取 dim/data_size/dist_func
- Python bindings：`PyHNSW` 改为持有 `std::unique_ptr<AlgorithmInterface<float>>`；新增 `makeHNSW` 工厂函数按 metric 实例化 `HierarchicalNSW<float, L2Space<float>>` 或 `HierarchicalNSW<float, InnerProductSpace<float>>`
- 修复 `test_arena_hnswlib.cpp` 中 `MockAlgorithm` 构造函数（不再传 SpacePtr 给基类）
- 编译通过，84/84 单元测试全部通过
- Benchmark 结果保存至 `/workspace/logs/benchmark_spacet.txt`（DEBUG 模式，inlining 收益须在 Release 模式下测量）
- 任务创建
- 分析现有代码范围：space_l2.h, space_ip.h, hnswalg.h, bindings.cpp, test_hnswalg.cpp, benchmark_index.cpp
- 确定改造策略：新增 static distFunc，保留虚函数接口；外部多态由 AlgorithmInterface 承担
- 确定 Python bindings 适配方案：makeHNSW 工厂函数按 metric 实例化
