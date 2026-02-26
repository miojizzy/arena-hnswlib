# [TASK011] - Fix AddPoint Performance (2x Slowdown vs hnswlib)

**Status:** Completed  
**Added:** 2026-02-26  
**Updated:** 2026-02-26

## Original Request
对比 hnswlib 官方库与当前项目的算法实现，跑性能测试发现当前项目的 AddPoint 操作慢了一倍，定位并修复原因。

## Thought Process
通过对比 hnswlib GitHub 源码（`mutuallyConnectNewElement` / `searchBaseLayer`）与项目代码（`updateExistPointAtLevel` / `getNeighborsByHeuristic2` / `LinkLists::init`），发现 3 处差异：

1. **层级分配 bug（正确性问题）**：`LinkLists::init()` 中的条件 `i < level_sizes[level - 1]` 写错了方向，导致 1000 元素的索引只有 2 个节点在 level ≥ 1（正确应该是 63 个），HNSW 实际退化为单层扁平图。

2. **`updateExistPointAtLevel` 未实现快速路径（主因）**：hnswlib 的 `mutuallyConnectNewElement` 在邻居列表未满时直接 append（O(1)，0 次距离计算）。旧代码无论列表是否满，都分配 `std::vector<bool>(elementSize_)` 做去重、把所有邻居入堆，再运行完整的 `getNeighborsByHeuristic2`。建索引早期（大部分节点邻接表未满），hnswlib 是 O(1)，旧代码是 O(M²)，差距极大。

3. **`getNeighborsByHeuristic2` 缺少 early-exit（次因）**：hnswlib 在候选数 ≤ M 时直接返回，避免无意义的堆排序和 O(N²) 距离计算。旧代码总是执行完整循环。

## Implementation Plan
- [x] 修复 `LinkLists::init()` 层级分配条件：`i < level_sizes[level - 1]` → `i >= level_sizes[level]`
- [x] 为 `updateExistPointAtLevel` 添加快速路径：列表未满直接 append 返回
- [x] 为 `getNeighborsByHeuristic2` 添加 early-exit：`if (top_candidates.size() <= M) return`
- [x] 删除 `updateExistPointAtLevel` 中无意义的 `std::vector<bool>(elementSize_)` 去重结构
- [x] 编译 + 全量测试（85/85 通过）
- [x] 更新 benchmark_release.txt

## Progress Tracking

**Overall Status:** Completed - 100%

### Subtasks
| ID | Description | Status | Updated | Notes |
|----|-------------|--------|---------|-------|
| 11.1 | 定位性能差距根因（对比 hnswlib 源码） | Complete | 2026-02-26 | 3处差异 |
| 11.2 | 修复层级分配 bug | Complete | 2026-02-26 | init() 条件 |
| 11.3 | updateExistPointAtLevel 快速路径 | Complete | 2026-02-26 | 列表未满直接 append |
| 11.4 | getNeighborsByHeuristic2 early-exit | Complete | 2026-02-26 | size <= M 提前返回 |
| 11.5 | 编译 & 测试 | Complete | 2026-02-26 | 85/85 passed |
| 11.6 | 更新 benchmark_release.txt | Complete | 2026-02-26 | BuildIndex -32% |

## Progress Log

### 2026-02-26
- 对比 hnswlib 官方仓库 `hnswalg.h` 与项目实现，定位到 3 处问题
- 修复 `LinkLists::init()` 层级分配逻辑（正确性 bug，影响召回率）：
  - 旧：`if (i < level_sizes[level - 1] && level > 0)` —— 边界方向错误
  - 新：`if (level > 0 && i >= level_sizes[level])` —— 按累计上界下降
  - 效果：1000 元素下 level≥1 节点数从 2 恢复到正确的 63
- 重写 `updateExistPointAtLevel`，添加列表未满快速路径（主性能修复）：
  - 旧：全程 `vector<bool>(elementSize_)` 去重 + 完整 heuristic
  - 新：`link_list.size < Mcurmax` → 直接 append；已满才 heuristic，候选池仅 M+1 个节点
- `getNeighborsByHeuristic2` 添加 early-exit（次要修复）：
  - `if (top_candidates.size() <= M) return top_candidates`
- 编译通过，85/85 单元测试全绿
- benchmark 结果（Release，1000×128d，M=16，ef=100）：
  - `HNSWFixture/BuildIndex`：60.8 ms → **41.5 ms（-32%）**
  - `HNSWFixture/QuerySingle`：29.6 µs → 31.4 µs（+6%，因层级结构修复后图更深，但 recall 更高）
