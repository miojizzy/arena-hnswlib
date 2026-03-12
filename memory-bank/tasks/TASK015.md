# TASK015 - 搜索调试统计与连通性分析

**状态：** 已完成
**添加日期：** 2026-03-11
**更新日期：** 2026-03-11

## 原始需求

为 search 和 addPoint 接口增加调试统计结构，用于分析：
1. 搜索过程中的距离计算次数、跳转次数等关键指标
2. 召回缺失点的原因分析（所需 ef、跳数等）
3. 图的连通性分析

## 需求详细分析

### 1. 搜索过程统计需求

**高层搜索（level >= 1，贪心下降阶段）：**
- 经过的层数
- 每层的距离计算次数、跳转次数
- 距离计算最多的层及次数
- 跳转最多的层及次数
- 高层总计

**底层搜索（level 0，beam search 阶段）：**
- 距离计算次数
- 访问过的节点数
- 访问节点中距入口点的最大跳数

### 2. 召回缺失分析需求

**场景举例：**
- 搜索点 t 的 top-k，返回 k-1 个命中，缺失点 p
- 需要知道：
  - 正常搜索最远走了多少跳
  - 点 p 距离 level 0 入口点多少跳
  - 需要多大的 ef 才能找到点 p

**目的：** 判断缺失是因为 ef 太小，还是图连通性问题

### 3. 连通性分析需求

**两种视角：**
1. 从入口节点(id=0)出发，每一层有多少无法连通的点
2. 从上一层所有节点出发（作为入口），每一层有多少无法连通的点

---

## 数据结构设计

### LevelSearchStats - 单层检索统计

```cpp
struct LevelSearchStats {
    size_t dist_calcs = 0;   // 该层距离计算次数
    size_t hops = 0;         // 该层跳转次数（贪心移动到更近点的次数）
};
```

### SearchDebugStats - 搜索过程统计

```cpp
struct SearchDebugStats {
    // ========== 高层统计（level >= 1，贪心下降阶段）==========
    size_t high_level_layers = 0;             // 经过的层数
    
    LevelSearchStats high_level_total;        // 高层总计
    //   - dist_calcs: 高层总距离计算次数
    //   - hops: 高层总跳转次数
    
    size_t high_level_dist_max_layer = 0;     // 距离计算最多的层号
    size_t high_level_dist_max_count = 0;     // 该层距离计算次数
    
    size_t high_level_hops_max_layer = 0;     // 跳转最多的层号
    size_t high_level_hops_max_count = 0;     // 该层跳转次数
    
    // 逐层详细统计（可选，按需填充）
    std::vector<LevelSearchStats> level_stats;
    
    // ========== 底层统计（level 0，beam search 阶段）==========
    LevelSearchStats base_level;              // level 0 统计
    //   - dist_calcs: 底层距离计算次数
    //   - hops: (beam search 无传统跳转概念，可记录扩展次数或其他)
    
    size_t visited_nodes = 0;                 // 访问过的节点数
    size_t max_hops_from_entry = 0;           // 访问节点中距入口点最大跳数
    
    // ========== 搜索路径记录（用于后续缺失分析）==========
    InternalId level0_entry_point = 0;        // 进入 level 0 的入口点ID
    std::vector<InternalId> visited_in_level0; // level 0 访问过的节点ID列表
};
```

### MissedPointAnalysis - 缺失点分析结果

```cpp
struct MissedPointAnalysis {
    InternalId point_id = 0;            // 缺失点的 internal id
    dist_t dist = 0;                    // 该点到查询点的距离
    size_t ground_truth_rank = 0;       // 该点在真实最近邻中的排名
    
    size_t hops_from_entry = 0;         // 从 level 0 入口点到该点的最短跳数
    size_t min_ef_to_reach = 0;         // 要找到该点需要的最小 ef
    bool was_visited = false;           // 搜索时是否访问过该节点（但被淘汰出 top-k）
    bool reachable = false;             // 该点是否在 level 0 图中可达
};
```

### LevelConnectivityStats - 单层连通性统计

```cpp
struct LevelConnectivityStats {
    size_t level = 0;                   // 层号
    size_t total_nodes = 0;             // 该层总节点数
    size_t reachable_nodes = 0;         // 可达节点数
    size_t unreachable_nodes = 0;       // 不可达节点数
    std::vector<InternalId> unreachable_ids;  // 不可达节点ID列表（可选，数据量大时可省略）
};
```

### ConnectivityReport - 连通性报告

```cpp
struct ConnectivityReport {
    // 从入口节点(id=0)出发的各层连通性
    // 索引 i 对应 level i
    std::vector<LevelConnectivityStats> from_entry;
    
    // 从上一层所有节点出发的各层连通性
    // 对于 level i，入口是 level i+1 的所有节点
    // 索引 i 对应 level i
    std::vector<LevelConnectivityStats> from_upper_layer;
};
```

### AddPointDebugStats - 插入调试统计（预留）

```cpp
struct AddPointDebugStats {
    // 预留，待后续需求明确后填充
    size_t level = 0;                   // 新点所在层级
    size_t total_dist_calcs = 0;        // 总距离计算次数
    // ... 其他指标待定
};
```

---

## 接口设计

### 搜索相关接口

```cpp
// 原有接口保持不变（无性能损耗）
BigTopPQueue<std::pair<dist_t, LabelType>>
searchKnn(const void* query_data, size_t k) const override;

// 新增：带调试统计的搜索
// 注意：会有额外开销（记录访问节点、计算跳数等）
std::pair<BigTopPQueue<std::pair<dist_t, LabelType>>, SearchDebugStats>
searchKnnWithStats(const void* query_data, size_t k) const;

// 新增：分析缺失点
// 需要提供缺失点ID、搜索统计、缺失点到查询点的距离
MissedPointAnalysis
analyzeMissedPoint(InternalId missed_id, 
                   const SearchDebugStats& stats,
                   dist_t query_dist) const;

// 新增：批量分析缺失点
std::vector<MissedPointAnalysis>
analyzeMissedPoints(const std::vector<InternalId>& missed_ids,
                    const SearchDebugStats& stats,
                    const dist_t* query_data) const;
```

### 连通性分析接口

```cpp
// 分析图的连通性
// 返回从入口节点和从上层节点出发的各层连通性报告
ConnectivityReport analyzeConnectivity() const;

// 可选：只分析从入口节点出发的连通性
std::vector<LevelConnectivityStats>
analyzeConnectivityFromEntry() const;
```

### 预留接口

```cpp
// 带调试统计的插入（预留）
void addPointWithStats(const void* data_point, LabelType label, 
                       AddPointDebugStats& stats);
```

---

## 实现方案

### 1. 高层统计实现

修改 `searchNearestAtLevel`，增加可选的统计参数：

```cpp
std::pair<dist_t, InternalId> 
searchNearestAtLevel(const void* query_data, 
                     InternalId nearest_id, 
                     dist_t nearest_dist, 
                     size_t level,
                     LevelSearchStats* stats = nullptr) const {
    auto changed = true;
    std::vector<InternalId> neighbors;
    while (changed) {
        changed = false;
        {
            std::unique_lock<std::mutex> lock(getLinkListMutex(nearest_id));
            const LinkListView& ll = link_lists_.getLinkList(nearest_id, level);
            neighbors.assign(ll.data, ll.data + ll.size);
        }
        for (InternalId candidate_id : neighbors) {
            if (stats) ++stats->dist_calcs;  // 记录距离计算
            dist_t dist = SpaceT::distFunc(...);
            if (dist < nearest_dist) {
                nearest_dist = dist;
                nearest_id = candidate_id;
                changed = true;
                if (stats) ++stats->hops;    // 记录跳转
            }
        }
    }
    return {nearest_dist, nearest_id};
}
```

### 2. 底层统计实现

修改 `searchKnnAtBaseLevel`，增加统计和路径记录：

```cpp
BigTopPQueue<std::pair<dist_t, InternalId>>
searchKnnAtBaseLevel(const void* query_data, 
                     InternalId enterpoint_id, 
                     dist_t enterpoint_dist, 
                     size_t k,
                     SearchDebugStats* stats = nullptr) const;
```

关键统计点：
- 每次调用 `distFunc` 时 `++dist_calcs`
- 每次访问新节点时记录到 `visited_in_level0`
- 搜索结束后，BFS 计算各访问节点的跳数

### 3. 缺失点分析实现

```cpp
MissedPointAnalysis analyzeMissedPoint(InternalId missed_id, 
                                        const SearchDebugStats& stats,
                                        dist_t query_dist) const {
    MissedPointAnalysis result;
    result.point_id = missed_id;
    result.dist = query_dist;
    
    // 1. BFS 计算跳数
    result.hops_from_entry = computeHopsFromEntry(stats.level0_entry_point, missed_id);
    
    // 2. 检查是否被访问过
    auto it = std::find(stats.visited_in_level0.begin(), 
                        stats.visited_in_level0.end(), missed_id);
    result.was_visited = (it != stats.visited_in_level0.end());
    
    // 3. 模拟 beam search 计算最小 ef
    if (!result.was_visited) {
        result.min_ef_to_reach = computeMinEfToReach(missed_id, query_dist, stats);
    }
    
    // 4. 可达性
    result.reachable = (result.hops_from_entry < SIZE_MAX);
    
    return result;
}
```

### 4. 连通性分析实现

```cpp
ConnectivityReport analyzeConnectivity() const {
    ConnectivityReport report;
    
    // 从入口节点出发
    for (size_t level = 0; level <= link_lists_.getMaxLevel(); ++level) {
        report.from_entry.push_back(analyzeLevelConnectivity(0, level));
    }
    
    // 从上层节点出发
    for (size_t level = 0; level <= link_lists_.getMaxLevel(); ++level) {
        if (level == link_lists_.getMaxLevel()) {
            // 最高层没有上层，用入口节点
            report.from_upper_layer.push_back(analyzeLevelConnectivity(0, level));
        } else {
            // 收集 level+1 层所有节点作为入口
            std::vector<InternalId> upper_nodes = getNodesAtLevel(level + 1);
            report.from_upper_layer.push_back(
                analyzeLevelConnectivityFromMultiple(upper_nodes, level));
        }
    }
    
    return report;
}
```

---

## 实现计划

### 子任务

| ID | 描述 | 状态 | 更新日期 | 备注 |
|----|------|------|----------|------|
| 1.1 | 定义调试统计数据结构 | 已完成 | 2026-03-11 | LevelSearchStats, SearchDebugStats, MissedPointAnalysis |
| 1.2 | 定义连通性分析数据结构 | 已完成 | 2026-03-11 | LevelConnectivityStats, ConnectivityReport |
| 1.3 | 实现 searchKnnWithStats | 已完成 | 2026-03-11 | 高层+底层统计 |
| 1.4 | 实现 analyzeMissedPoint | 已完成 | 2026-03-11 | 缺失点分析 |
| 1.5 | 实现 analyzeConnectivity | 已完成 | 2026-03-11 | 连通性分析 |
| 1.6 | 编写单元测试 | 已完成 | 2026-03-11 | 4个新测试用例，85测试全通过 |
| 1.7 | 提交代码 | 已完成 | 2026-03-12 | 已提交并推送 |

---

## 使用示例

```cpp
// 1. 带统计的搜索
auto [results, stats] = index.searchKnnWithStats(query_data, k);

// 2. 与 ground truth 对比
std::vector<InternalId> ground_truth = getGroundTruth(query_data, k);
std::vector<InternalId> missed = findMissedPoints(results, ground_truth);

// 3. 分析缺失点
for (InternalId missed_id : missed) {
    dist_t dist = computeDistance(query_data, missed_id);
    auto analysis = index.analyzeMissedPoint(missed_id, stats, dist);
    
    std::cout << "缺失点 " << missed_id 
              << ": 跳数=" << analysis.hops_from_entry
              << ", 最小ef=" << analysis.min_ef_to_reach
              << ", 曾访问过=" << analysis.was_visited << "\n";
}

// 4. 连通性分析
auto connectivity = index.analyzeConnectivity();
for (const auto& level_stats : connectivity.from_entry) {
    std::cout << "Level " << level_stats.level 
              << ": 不可达节点 " << level_stats.unreachable_nodes
              << "/" << level_stats.total_nodes << "\n";
}
```

---

## 调试功能详细说明

### 适用场景

| 场景 | 推荐功能 | 说明 |
|------|----------|------|
| 召回率分析 | `searchKnnWithStats` + `analyzeMissedPoint` | 分析缺失点是 ef 问题还是连通性问题 |
| 索引健康检查 | `analyzeConnectivity` | 检测图中是否存在孤立节点 |
| 性能调优 | `searchKnnWithStats` | 分析搜索过程的开销分布 |
| ef 参数选择 | `analyzeMissedPoint` | 确定合适的 efSearch 值 |

### 典型使用流程

#### 场景一：召回缺失分析

当搜索结果的召回率不达标时，可用以下流程分析原因：

```cpp
// Step 1: 构建索引
HierarchicalNSW<float, InnerProductSpace<float>> index(...);
for (size_t i = 0; i < n; ++i) {
    index.addPoint(data[i], i);
}

// Step 2: 对测试查询执行带统计的搜索
auto [results, stats] = index.searchKnnWithStats(query, k);

// Step 3: 与 ground truth 对比，找出缺失点
std::set<uint32_t> result_labels;
while (!results.empty()) {
    result_labels.insert(results.top().second);
    results.pop();
}
std::vector<InternalId> missed_ids;
for (auto gt_label : ground_truth) {
    if (result_labels.find(gt_label) == result_labels.end()) {
        missed_ids.push_back(label_to_internal_id[gt_label]);
    }
}

// Step 4: 分析缺失原因
for (InternalId missed_id : missed_ids) {
    float dist = computeDistance(query, data[missed_id]);
    auto analysis = index.analyzeMissedPoint(missed_id, stats, dist);
    
    if (!analysis.reachable) {
        std::cout << "缺失点 " << missed_id << ": 图中不可达！连通性问题。\n";
    } else if (analysis.was_visited) {
        std::cout << "缺失点 " << missed_id << ": 已访问但被淘汰，考虑增大 k 或调整距离度量。\n";
    } else if (analysis.min_ef_to_reach > current_ef) {
        std::cout << "缺失点 " << missed_id << ": ef 太小，需要 ef >= " 
                  << analysis.min_ef_to_reach << "（当前 ef=" << current_ef << "）\n";
    } else {
        std::cout << "缺失点 " << missed_id << ": 跳数=" << analysis.hops_from_entry 
                  << "，可能需要调整索引参数 (M, efConstruction)。\n";
    }
}
```

#### 场景二：索引健康检查

定期检查索引连通性，确保图结构正常：

```cpp
auto report = index.analyzeConnectivity();

std::cout << "=== 连通性报告 ===\n";
std::cout << "\n从入口节点出发:\n";
for (const auto& ls : report.from_entry) {
    std::cout << "  Level " << ls.level << ": " 
              << ls.reachable_nodes << "/" << ls.total_nodes << " 可达";
    if (ls.unreachable_nodes > 0) {
        std::cout << " (警告: " << ls.unreachable_nodes << " 不可达)";
    }
    std::cout << "\n";
}

std::cout << "\n从上层节点出发:\n";
for (const auto& ls : report.from_upper_layer) {
    std::cout << "  Level " << ls.level << ": "
              << ls.reachable_nodes << "/" << ls.total_nodes << " 可达";
    if (ls.unreachable_nodes > 0) {
        std::cout << " (警告: " << ls.unreachable_nodes << " 不可达)";
    }
    std::cout << "\n";
}

// 严重问题：level 0 有不可达节点
if (report.from_entry[0].unreachable_nodes > 0) {
    std::cerr << "严重警告: Level 0 存在不可达节点，搜索将永远无法找到这些点！\n";
    std::cerr << "不可达节点 ID: ";
    for (auto id : report.from_entry[0].unreachable_ids) {
        std::cerr << id << " ";
    }
    std::cerr << "\n";
}
```

#### 场景三：性能分析

分析搜索开销分布：

```cpp
auto [results, stats] = index.searchKnnWithStats(query, k);

std::cout << "=== 搜索性能分析 ===\n";

// 高层开销
if (stats.high_level_layers > 0) {
    std::cout << "高层贪心下降:\n";
    std::cout << "  经过层数: " << stats.high_level_layers << "\n";
    std::cout << "  总距离计算: " << stats.high_level_total.dist_calcs << "\n";
    std::cout << "  总跳转次数: " << stats.high_level_total.hops << "\n";
    std::cout << "  距离计算最多的层: " << stats.high_level_dist_max_layer 
              << " (" << stats.high_level_dist_max_count << " 次)\n";
    std::cout << "  跳转最多的层: " << stats.high_level_hops_max_layer 
              << " (" << stats.high_level_hops_max_count << " 次)\n";
}

// 底层开销
std::cout << "底层 Beam Search:\n";
std::cout << "  距离计算: " << stats.base_level.dist_calcs << "\n";
std::cout << "  访问节点数: " << stats.visited_nodes << "\n";
std::cout << "  最大跳数: " << stats.max_hops_from_entry << "\n";

// 开销占比
size_t total_dist = stats.high_level_total.dist_calcs + stats.base_level.dist_calcs;
float high_ratio = 100.0 * stats.high_level_total.dist_calcs / total_dist;
std::cout << "开销分布: 高层 " << high_ratio << "%, 底层 " << (100 - high_ratio) << "%\n";
```

### 性能开销说明

| 功能 | 额外开销 | 建议使用时机 |
|------|----------|--------------|
| `searchKnnWithStats` | 约 5-15% | 调试、性能分析时 |
| `analyzeMissedPoint` | O(N) BFS | 仅在召回缺失时按需调用 |
| `analyzeConnectivity` | O(N×E) 全图遍历 | 索引构建后一次性检查 |

**重要提示：**
- `searchKnn` 无额外开销，生产环境推荐使用
- `searchKnnWithStats` 会记录访问路径，有内存和计算开销
- `analyzeMissedPoint` 内部使用 BFS 计算跳数，对大图较慢
- `analyzeConnectivity` 是最昂贵的操作，建议离线使用

### 统计指标解读

#### SearchDebugStats 关键指标

| 指标 | 含义 | 理想范围 | 异常信号 |
|------|------|----------|----------|
| `high_level_total.dist_calcs` | 高层距离计算总次数 | < 100 | 层数太多或每层邻居太多 |
| `high_level_total.hops` | 高层贪心跳转次数 | < 20 | 图结构可能过于稀疏 |
| `visited_nodes` | 底层访问节点数 | 接近 efSearch | 远大于 efSearch 可能有冗余 |
| `max_hops_from_entry` | 最大跳数 | < 10 | 跳数过大说明搜索路径长 |

#### MissedPointAnalysis 关键指标

| 指标 | 含义 | 判断逻辑 |
|------|------|----------|
| `reachable` | 是否可达 | false → 图连通性问题 |
| `was_visited` | 是否被访问过 | true → 被 top-k 淘汰，考虑增大 k |
| `min_ef_to_reach` | 最小 ef | > efSearch → 需要增大 efSearch |
| `hops_from_entry` | 跳数 | 大值 → 该点距离入口远，可能需要调整入口策略 |

#### ConnectivityReport 关键指标

| 指标 | 含义 | 异常处理 |
|------|------|----------|
| `from_entry[i].unreachable_nodes` | 从入口不可达数 | > 0 特别是 level 0 → 索引有问题 |
| `from_upper_layer[i].unreachable_nodes` | 从上层不可达数 | 高于 from_entry → 层间连接问题 |

### 常见问题诊断

```
问题：召回率低
├── 检查连通性
│   └── analyzeConnectivity() → unreachable_nodes > 0?
│       ├── 是 → 图结构问题，检查 efConstruction 和 M
│       └── 否 → 继续分析
├── 分析缺失点
│   └── analyzeMissedPoint() 
│       ├── !reachable → 图连通性问题（罕见）
│       ├── was_visited → top-k 太小，增大 k
│       └── min_ef > efSearch → 增大 efSearch
└── 性能分析
    └── searchKnnWithStats() → max_hops 过大?
        └── 是 → 考虑重建索引，增大 M 或 efConstruction
```

---

## 进度日志

### 2026-03-11
- 创建 Task 文档
- 完成数据结构和接口设计
- 创建分支 feature/TASK015-search-debug-stats
- 开始实现数据结构定义

### 2026-03-12
- 所有子任务完成
- 代码已提交 (commit ec46e84)
- 代码已推送到远端
- 完善调试功能详细说明文档
