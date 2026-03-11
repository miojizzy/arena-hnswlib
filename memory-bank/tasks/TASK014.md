# TASK014 - 优化 getNeighborsByHeuristic2 堆操作

**状态：** 已完成
**添加日期：** 2026-03-11
**更新日期：** 2026-03-11

## 原始需求
分析 `getNeighborsByHeuristic2` 函数，发现其中使用了一个不必要的堆 `queue_closest`，可以优化掉。

## 思考过程
分析代码发现：
1. `top_candidates` 是大顶堆（距离最大在堆顶）
2. `queue_closest` 通过取负距离转换成小顶堆（距离最小在堆顶）
3. `queue_closest` 仅用于按距离从小到大遍历候选者，并非真正的优先队列操作

问题：`queue_closest` 完全是不必要的堆结构！它只是用于顺序遍历，构建堆的代价是 O(N log N)，而用排序后的 vector 可以达到同样的效果，且仅需 O(N)。

## 实现计划
- [x] 将 `queue_closest` 替换为 `std::vector` + `std::reverse`
- [x] 运行单测验证正确性
- [x] 运行 benchmark 验证性能

## 进度追踪

**整体状态：** 已完成 - 100%

### 子任务
| ID | 描述 | 状态 | 更新日期 | 备注 |
|----|------|------|----------|------|
| 1.1 | 移除 queue_closest 堆，改用 vector + reverse | 已完成 | 2026-03-11 | 代码简化，性能提升 |
| 1.2 | 运行单测验证 | 已完成 | 2026-03-11 | 85 个测试全部通过 |
| 1.3 | 运行 benchmark 验证 | 已完成 | 2026-03-11 | benchmark_index 运行正常 |

## 进度日志
### 2026-03-11
- 分析函数发现 `queue_closest` 堆不必要
- 重构实现：用 vector 替代堆
- 优化对比：
  - 优化前：O(N log N) 建堆 + O(N log N) 遍历
  - 优化后：O(N) 复制 + O(N) 反转
- 代码更简洁，负距离转换的 hack 也被移除
- 单测：85/85 通过
- Benchmark：
  - BuildIndex: 39.4 ms
  - QuerySingle: 0.032 ms
  - QueryBatch: 0.315 ms
