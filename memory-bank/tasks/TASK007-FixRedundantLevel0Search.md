# TASK007 - 修复 searchKnn 中 Level 0 被重复搜索的问题

**状态：** 已完成  
**添加日期：** 2026-02-24  
**更新日期：** 2026-02-24

## 原始需求

性能分析发现 `searchKnn` 中存在逻辑错误：greedy 下降循环对 level 0 执行了一次 `searchNearestAtLevel`，随后 `searchKnnAtBaseLevel` 又在 level 0 执行完整的 beam search——level 0 被搜了两次。

## 思考过程

### 当前代码逻辑

```cpp
BigTopPQueue<std::pair<dist_t, LabelType>>
searchKnn(const void* query_data, size_t k) const override {
    InternalId enterpoint_id = 0;
    auto nearest_id = enterpoint_id;
    auto nearest_dist = dist_func_(..., enterpoint_id, dim_);

    // 问题：level-- > 0 当 level=0 时也会执行 searchNearestAtLevel(... level=0)
    for (size_t level = link_lists_.getMaxLevel() + 1; level-- > 0; ) {
        std::tie(nearest_dist, nearest_id) = searchNearestAtLevel(query_data, nearest_id, nearest_dist, level);
    }

    // 然后 level 0 又做了完整 beam search
    auto topCandidates = searchKnnAtBaseLevel(query_data, nearest_id, nearest_dist, k);
```

循环 `level-- > 0` 的展开：
- 初始 level = maxLevel + 1（例如 maxLevel=2，则从 3 开始）
- 先 level-- → 2，执行 searchNearestAtLevel(level=2) ✓
- 再 level-- → 1，执行 searchNearestAtLevel(level=1) ✓
- 再 level-- → 0，执行 searchNearestAtLevel(level=0) ← **多余**
- 再 level-- → wraps to max uint（unsigned underflow），循环结束

然后再调 `searchKnnAtBaseLevel` 对 level=0 做 beam search。

### 标准 HNSW 算法

按照原始论文（Malkov & Yashunin 2018）的伪代码：
```
SEARCH-LAYER(q, ep, ef, lc):
  用在 lc 层做 beam search

KNN-SEARCH(q, K, ef):
  ep ← 入口点
  for lc = L downto 1:  // 只到 level 1
      ep ← SEARCH-LAYER(q, ep, ef=1, lc)  // greedy search
  W ← SEARCH-LAYER(q, ep, ef, lc=0)       // beam search at level 0
  return K nearest
```

greedy 下降只需走到 level 1，level 0 只做一次 beam search。

### 修复方案

将循环条件从 `level-- > 0` 改为 `level-- > 1`（或者等效地，循环只在 level > 0 时执行 greedy，跳过 level 0）：

```cpp
// 修复后：greedy 只搜 level >= 1
for (size_t level = link_lists_.getMaxLevel() + 1; level-- > 1; ) {
    std::tie(nearest_dist, nearest_id) = searchNearestAtLevel(query_data, nearest_id, nearest_dist, level);
}
// level 0 只做一次 beam search
auto topCandidates = searchKnnAtBaseLevel(query_data, nearest_id, nearest_dist, k);
```

### 特殊情况处理

- **maxLevel = 0**（所有元素都在 level 0）：循环从 level=1 开始，`level-- > 1` → level=0，条件 `0 > 1` 为 false，不执行，直接进入 beam search。✓
- **只有 1 个元素**：同上，直接 beam search。✓
- **size_t 下溢**：无需担心，当 level=1 时 `level-- > 1` → level=0，条件 false，循环退出，不会下溢。✓

### 性能预期

`searchNearestAtLevel` 在 level 0 的开销比其他层大得多（level 0 的节点数最多，M0 = 2M = 32 邻居），消除一次 level 0 的 greedy 走步可减少约 5~15% 的查询时间（视图结构而定）。

此外，当前 greedy 结果（level 0 出口）作为 beam search 的 enterpoint——将 greedy 提前终止在 level 1，beam search 从 level 1 的最近邻进入 level 0，功能等价且 **入口质量不变**。

### 和 TASK006 的关系

两个任务独立，可以并行进行。TASK007 的改动极小（一行代码），建议先做。TASK006 是大头优化。

## 实现计划

- [ ] 1. 修改 `searchKnn` 中 greedy 下降循环条件：`level-- > 0` → `level-- > 1`

- [ ] 2. 添加注释说明 level 0 由 `searchKnnAtBaseLevel` 负责

- [ ] 3. 验证 maxLevel=0 边界情况下行为正确（单元测试）

- [ ] 4. 运行 benchmark 记录性能变化

## 进度追踪

**整体状态：** 已完成 - 100%

### 子任务
| ID | 描述 | 状态 | 更新日期 | 备注 |
|----|------|------|----------|------|
| 7.1 | 修改循环条件 | 已完成 | 2026-02-24 | level-- > 0 → level-- > 1，加注释 |
| 7.2 | 补充注释 | 已完成 | 2026-02-24 | |
| 7.3 | 单元测试验证 | 已完成 | 2026-02-24 | 全部 84 个测试通过 |
| 7.4 | Benchmark 对比 | 已完成 | 2026-02-24 | 与 TASK006 合并测量 |

## 进度日志
### 2026-02-24
- 任务创建，分析 level 0 重复搜索根本原因，验证边界情况正确性
### 2026-02-24（完成）
- 将 searchKnn 中循环条件 `level-- > 0` 改为 `level-- > 1`
- 添加注释说明 level 0 由 searchKnnAtBaseLevel 单独处理
- 与 TASK006 一同提交，全部 84 个测试通过
