# TASK006 - 用 Bitmap 替换 visited std::set 优化搜索性能

**状态：** 已完成  
**添加日期：** 2026-02-24  
**更新日期：** 2026-02-24

## 原始需求

性能分析发现 `searchKnnAtBaseLevel`、`updateNewPointAtLevel`、`updateExistPointAtLevel` 中均使用 `std::set<InternalId>` 作为 visited 集合，导致大量堆内存分配和 cache miss，是当前 HNSW 搜索/建图最主要的性能瓶颈。

## 思考过程

### 问题根源

`std::set` 基于红黑树实现：
- `find` / `insert` 均为 O(log n)，且每个节点是独立堆分配
- 遍历时指针跳转，破坏 CPU 缓存局部性，产生大量 L1/L2 cache miss
- 每次函数调用都要构造 + 析构整个 set，涉及反复分配/释放

### 解决方案对比

| 方案 | 查询 | 插入 | 重置 | 内存 | Cache 友好性 |
|------|------|------|------|------|------|
| `std::set` | O(log n) | O(log n) | O(n) destroy | 分散堆分配 | 差 |
| `std::unordered_set` | O(1) avg | O(1) avg | O(n) destroy | 分散堆分配 | 一般 |
| `vector<bool>` bitmap + dirty list | O(1) | O(1) | O(visited count) | 单块连续内存 | 极好 |

**选择方案：预分配 bitmap + dirty list**

设计：
- `VisitedTable` 类：持有大小为 `elementSize_` 的 `std::vector<bool>`（或 `vector<uint8_t>` 以避免 `vector<bool>` 位压缩的寻址开销）
- 同时维护一个 `std::vector<InternalId> dirty_list_` 记录本次已标记的节点
- `mark(id)` → O(1) 写，`isVisited(id)` → O(1) 读
- `reset()` → 遍历 dirty_list_ 清零，O(visited count)，不需要整表 memset

### 使用 `uint8_t` 而非 `bool`

`std::vector<bool>` 使用位压缩，单个 bit 的读写需要 mask 操作，且不能取地址。改用 `std::vector<uint8_t>` 每元素 1 字节，对于 1000 个节点只占 ~1KB，完全在 L1 cache 内，寻址简单。

### 生命周期管理

当前三个热路径函数都是局部构造 visited：
```cpp
// 现在
std::set<InternalId> visited;

// 目标：复用预分配的 VisitedTable
VisitedTable visited_table(elementSize_);  // HierarchicalNSW 成员
```

在 `searchKnnAtBaseLevel` 和 `updateNewPointAtLevel` 调用前后执行 `reset()`。

**线程安全**：当前实现单线程，`VisitedTable` 作为成员变量可直接复用，不需要额外同步。若未来支持并发需换成 thread_local 或 per-query 分配。

## 实现计划

- [ ] 1. 在 `hnswalg.h` 中添加 `VisitedTable` 辅助类
  - 内部使用 `std::vector<uint8_t>` 存储标记
  - 提供 `mark(id)`、`isVisited(id)`、`reset()` 接口
  - 构造时预分配 `elementSize_` 大小

- [ ] 2. `HierarchicalNSW` 类增加 `VisitedTable visited_table_` 成员
  - 在构造函数初始化列表中初始化

- [ ] 3. 替换 `searchKnnAtBaseLevel` 中的 `std::set<InternalId> visited`
  - 改为使用 `visited_table_`，在函数头调用 `reset()`

- [ ] 4. 替换 `updateNewPointAtLevel` 中的 `std::set<InternalId> visited`
  - 改为使用 `visited_table_`，在函数头调用 `reset()`

- [ ] 5. 替换 `updateExistPointAtLevel` 中的 `std::set<InternalId> seen`
  - 改为使用 `visited_table_`，在函数头调用 `reset()`

- [ ] 6. 运行单元测试确认正确性（`tests/unit/test_hnswalg.cpp`）

- [ ] 7. 运行 benchmark 对比优化前后性能数据

## 进度追踪

**整体状态：** 已完成 - 100%

### 子任务
| ID | 描述 | 状态 | 更新日期 | 备注 |
|----|------|------|----------|------|
| 6.1 | 实现 VisitedTable 类 | 已完成 | 2026-02-24 | uint8_t bitmap + dirty list |
| 6.2 | HierarchicalNSW 增加 mutable 成员 | 已完成 | 2026-02-24 | mutable for const search methods |
| 6.3 | 替换 searchKnnAtBaseLevel | 已完成 | 2026-02-24 | |
| 6.4 | 替换 updateNewPointAtLevel | 已完成 | 2026-02-24 | |
| 6.5 | 替换 updateExistPointAtLevel | 已完成 | 2026-02-24 | vector<bool> local_seen（bounded by M+1）|
| 6.6 | 单元测试通过 | 已完成 | 2026-02-24 | 全部 84 个测试通过 |
| 6.7 | Benchmark 对比验证 | 已完成 | 2026-02-24 | QuerySingle 187µs → 29µs（6.4x faster）|

## 进度日志
### 2026-02-24
- 任务创建，完成性能瓶颈分析，确定 std::set 为主要瓶颈，设计 VisitedTable 方案
### 2026-02-24（完成）
- 在 hnswalg.h 中 namespace 顶部添加 VisitedTable 类（uint8_t table_ + dirty_ vector）
- HierarchicalNSW 增加 `mutable VisitedTable visited_table_` 成员（mutable 使 const 搜索方法可修改）
- 替换 searchKnnAtBaseLevel 中 std::set → visited_table_.reset()/mark()/isVisited()
- 替换 updateNewPointAtLevel 中 std::set → visited_table_.reset()/mark()/isVisited()
- updateExistPointAtLevel 候选池大小有界（≤ M+1），使用 local vector<bool> 替换 std::set
- 添加 #include <set> 以保留 getTwoHopNeighborsOnLevel 中的 std::set
- 全部 84 个测试通过
- Benchmark 结果：
  - HNSWFixture/BuildIndex：171ms → 63ms（**2.7x faster**）
  - HNSWFixture/QuerySingle：187µs → 29µs（**6.4x faster**）
  - BM_HNSW_QueryVaryingK/1：33.9µs → 8.81µs（**3.8x faster**）
  - BM_HNSW_VaryingDataSize/1000：74.8µs → 13.2µs（**5.7x faster**）
- 结果文件：logs/benchmark_before.txt, logs/benchmark_after.txt
