# [TASK010] - VisitedTable 改为请求粒度临时变量

**Status:** Completed  
**Added:** 2026-02-25  
**Updated:** 2026-02-25

## Original Request
将 `HierarchicalNSW` 中 `VisitedTable visited_table_` 由共享成员变量改为请求粒度的局部临时变量，避免多线程并发搜索时出现数据竞争冲突。

## Thought Process
当前实现中 `visited_table_` 是 `HierarchicalNSW` 的 `mutable` 成员，在 `searchKnnAtBaseLevel`（只读搜索路径）和 `updateNewPointAtLevel`（构建路径）中被复用，目的是减少 `VisitedTable` 的频繁分配开销。

但这导致：
- 多线程并发调用 `searchKnn` 时，所有线程共享同一个 `visited_table_`，产生数据竞争（race condition）
- 测试3 `ProducerConsumerBatchInsert` 中已有注释说明这一已知缺陷

**修改方案**：
- 删除类成员 `mutable VisitedTable visited_table_`
- 在 `searchKnnAtBaseLevel` 和 `updateNewPointAtLevel` 两个函数入口处各自创建一个局部 `VisitedTable`
- 内存容量 = `elementSize_`（internal ID 空间，等于构建时预分配的最大元素数）
- 性能代价：每次搜索/插入多一次 `vector<uint8_t>` 的栈上创建，但 `VisitedTable` reserve(256) 的 dirty list 开销极小

## Implementation Plan
- [x] 创建任务文件  
- [x] 删除 `HierarchicalNSW` 的 `mutable VisitedTable visited_table_` 成员及构造函数初始化  
- [x] `updateNewPointAtLevel` 顶部改为 `VisitedTable visited_table(elementSize_);`（去掉 `.reset()`）  
- [x] `searchKnnAtBaseLevel` 顶部改为 `VisitedTable visited_table(elementSize_);`（去掉 `.reset()`）  
- [x] 更新并发测试：移除"VisitedTable 竞争"注释，改为真正并发搜索测试  
- [x] 编译+全部测试通过  
- [x] VisitedTable 改为 bitmap（uint64_t），删除 reset()/dirty_，QueryBatch -3.0%  

## Progress Tracking

**Overall Status:** Completed - 100%

### Subtasks
| ID | Description | Status | Updated | Notes |
|----|-------------|--------|---------|-------|
| 10.1 | 删除成员变量与构造初始化 | Complete | 2026-02-25 | |
| 10.2 | updateNewPointAtLevel 改为局部 VisitedTable | Complete | 2026-02-25 | |
| 10.3 | searchKnnAtBaseLevel 改为局部 VisitedTable | Complete | 2026-02-25 | |
| 10.4 | 更新并发测试 | Complete | 2026-02-25 | 测试3改为真正并发搜索 |
| 10.5 | 编译与全量测试通过 | Complete | 2026-02-25 | 9/9 passed |
| 10.6 | VisitedTable 改为 bitmap（uint64_t） | Complete | 2026-02-25 | QueryBatch -3.0%，内存 8x 更小 |

## Progress Log
### 2026-02-25
- 创建任务文件，确定实现方案
- 识别两处使用点：`updateNewPointAtLevel` 和 `searchKnnAtBaseLevel`
- 删除 `HierarchicalNSW` 成员 `mutable VisitedTable visited_table_` 及构造初始化
- 两个函数改为各自创建局部 `VisitedTable visited_table(elementSize_)`
- 清理 `updateExistPointAtLevel` 中过时注释
- 将 `test_concurrent.cpp` 测试3 改为真正 8 线程并发搜索，新增 `test_concurrent` 到 CMakeLists
- 全部 9 个单元测试通过，无 error/warning
- 进一步将 `VisitedTable` 底层从 `vector<uint8_t>` 改为 `vector<uint64_t>` bitmap
  - 删除 `dirty_`、`reserve(256)`、`mark()` 中的 push_back、`reset()` 方法
  - `mark(id)`: `table_[id>>6] |= (1ULL << (id&63))`
  - `isVisited(id)`: `(table_[id>>6] >> (id&63)) & 1`
- Release benchmark：QuerySingle -2.4%，QueryBatch -3.0%，Build -1.2%
- 更新 `logs/benchmark_release.txt` 为最新性能基线
