#pragma once

#include <cstddef>
#include "arena_hnswlib.h"
#include "data_store.h"
#include <random>
#include <stdexcept>
#include <numeric>
#include <set>
#include <algorithm>
#include <atomic>
#include <array>
#include <mutex>
#include <vector>
#include <queue>
#include <limits>
#include <unordered_map>

namespace arena_hnswlib {

// ---------------------------------------------------------------------------
// 调试统计数据结构
// ---------------------------------------------------------------------------

// 单层检索统计
struct LevelSearchStats {
  size_t dist_calcs = 0;   // 该层距离计算次数
  size_t hops = 0;         // 该层跳转次数（贪心移动到更近点的次数）
};

// 搜索过程统计
struct SearchDebugStats {
  // ========== 高层统计（level >= 1，贪心下降阶段）==========
  size_t high_level_layers = 0;             // 经过的层数

  LevelSearchStats high_level_total;        // 高层总计

  size_t high_level_dist_max_layer = 0;     // 距离计算最多的层号
  size_t high_level_dist_max_count = 0;     // 该层距离计算次数

  size_t high_level_hops_max_layer = 0;     // 跳转最多的层号
  size_t high_level_hops_max_count = 0;     // 该层跳转次数

  // 逐层详细统计
  std::vector<LevelSearchStats> level_stats;

  // ========== 底层统计（level 0，beam search 阶段）==========
  LevelSearchStats base_level;              // level 0 统计

  size_t visited_nodes = 0;                 // 访问过的节点数
  size_t max_hops_from_entry = 0;           // 访问节点中距入口点最大跳数

  // ========== 搜索路径记录（用于后续缺失分析）==========
  InternalId level0_entry_point = 0;        // 进入 level 0 的入口点ID
  std::vector<InternalId> visited_in_level0; // level 0 访问过的节点ID列表
};

// 缺失点分析结果
struct MissedPointAnalysis {
  InternalId point_id = 0;            // 缺失点的 internal id
  double dist = 0.0;                  // 该点到查询点的距离
  size_t ground_truth_rank = 0;       // 该点在真实最近邻中的排名

  size_t hops_from_entry = 0;         // 从 level 0 入口点到该点的最短跳数
  size_t min_ef_to_reach = 0;         // 要找到该点需要的最小 ef
  bool was_visited = false;           // 搜索时是否访问过该节点（但被淘汰出 top-k）
  bool reachable = false;             // 该点是否在 level 0 图中可达
};

// 单层连通性统计
struct LevelConnectivityStats {
  size_t level = 0;                   // 层号
  size_t total_nodes = 0;             // 该层总节点数
  size_t reachable_nodes = 0;         // 可达节点数
  size_t unreachable_nodes = 0;       // 不可达节点数
  std::vector<InternalId> unreachable_ids;  // 不可达节点ID列表
};

// 连通性报告
struct ConnectivityReport {
  // 从入口节点(id=0)出发的各层连通性
  std::vector<LevelConnectivityStats> from_entry;

  // 从上一层所有节点出发的各层连通性
  std::vector<LevelConnectivityStats> from_upper_layer;
};

// 插入调试统计（预留）
struct AddPointDebugStats {
  size_t level = 0;                   // 新点所在层级
  size_t total_dist_calcs = 0;        // 总距离计算次数
};

// ---------------------------------------------------------------------------
// Tuning knobs
// ---------------------------------------------------------------------------

// Number of lock-striping buckets for neighbor-list concurrency.
// Node id maps to bucket via  id & (kLockStripes - 1)  (must be a power of 2).
// Memory cost: kLockStripes × sizeof(std::mutex) = 65536 × 40 B ≈ 2.5 MB.
// Collision probability per pair of nodes: 1 / kLockStripes ≈ 0.0015 %.
static constexpr size_t kLockStripes = 65536;

// ---------------------------------------------------------------------------
// 0层邻居选择模式
enum class Layer0NeighborMode {
  kDoubleM,              // 0层使用 2*M 个启发式邻居（默认，现有逻辑）
  kHeuristicOnly,        // 0层只使用 M 个启发式邻居
  kHeuristicPlusClosest  // 0层使用 M 个启发式邻居 + M 个最近距离邻居
};

// Visited-set backed by a bitmap (1 bit per element).
// 8x smaller than a uint8_t array → better cache utilization for large indexes.
// Zero-initialization cost is also 8x lower on each per-request construction.
class VisitedTable {
    std::vector<uint64_t> table_;

 public:
    explicit VisitedTable(size_t capacity)
        : table_((capacity + 63) / 64, 0) {}

    inline void mark(InternalId id) {
        table_[id >> 6] |= (uint64_t(1) << (id & 63));
    }

    inline bool isVisited(InternalId id) const {
        return (table_[id >> 6] >> (id & 63)) & 1;
    }
};


struct LinkListView {
    uint32_t size;      ///< 当前邻居数量
    InternalId data[];  ///< 邻居列表柔性数组
};

class LinkLists {
    const size_t M_, M0_;
    const Layer0NeighborMode mode_;
    const size_t elementSize_;

    size_t maxLevel_;
    std::unique_ptr<InternalId[]> data_;
    std::vector<size_t> internal_id_level_; // internal id -> level
    std::vector<InternalId*> level_data_; // level -> 起始地址


    void init() {
        // 根据整体数量计算每一层的元素数量
        std::vector<size_t> level_sizes; // 每一层的元素数量
        level_sizes.emplace_back(elementSize_); // 所有元素都在0层
        // 每层的数量递减，按照 M_ 比例递减
        while(level_sizes.back() > M_) {
            auto num = static_cast<size_t>(std::floor(level_sizes.back() * 1.0 / M_));
            level_sizes.emplace_back(num);
        }
        maxLevel_ = level_sizes.size() - 1;
        // 确保入口节点(id=0)是最高层唯一节点：
        // while 循环以 > M_ 为条件结束，最终 level_sizes.back() 可能等于 M_（如2），
        // 导致最高层有多个节点。强制设为1，保证 id=0 是最高层的唯一入口。
        if (maxLevel_ > 0) {
            level_sizes.back() = 1;
        }

        // 分配 internal id 到 level 的映射
        // 按照 level_sizes 的累计边界将 internal id 分配到对应层级：
        //   ids [0,          level_sizes[maxLevel_]) → maxLevel_
        //   ids [level_sizes[maxLevel_], level_sizes[maxLevel_-1]) → maxLevel_-1
        //   ...
        //   ids [level_sizes[1], level_sizes[0]) → 0
        internal_id_level_.resize(elementSize_, 0);
        auto level = maxLevel_;
        for(auto i = 0; i < elementSize_; ++i) {
            // 当 i 跨过当前层级的上界时下降一层
            if (level > 0 && i >= level_sizes[level]) {
                --level;
            }
            internal_id_level_[i] = level;
        }

        // 分配邻居列表存储空间
        size_t total_size = 0;
        for(auto level = 0; level <= maxLevel_; ++level) {
            total_size += level_sizes[level] * getLinkListSize(level);
        }
        data_ = std::make_unique<InternalId[]>(total_size);

        // 计算每一层的起始地址
        level_data_.resize(maxLevel_ + 1, nullptr);
        level_data_[maxLevel_] = data_.get();
        for (auto level = maxLevel_ ; level-- > 0; ) {
            auto last_level = level + 1;
            level_data_[level] = level_data_[last_level] + level_sizes[last_level] * getLinkListSize(last_level);
        }
    }

 public:
    inline const size_t& getM(size_t level) const {
        return level == 0 ? M0_ : M_;
    }
    
    inline const size_t getLinkListSize(uint32_t level) const {
        return getM(level) + 1;
    }

    inline Layer0NeighborMode getMode() const { return mode_; }

 public:
    LinkLists(size_t M, size_t elementSize, 
              Layer0NeighborMode mode = Layer0NeighborMode::kDoubleM) 
        : M_(M), 
          M0_(mode == Layer0NeighborMode::kHeuristicOnly ? M : 2*M),
          mode_(mode),
          elementSize_(elementSize){
        init();
    }
    
    inline const size_t & getMaxLevel() const {
        return maxLevel_;
    }

    inline const size_t& getLevel(InternalId id) const {
        return internal_id_level_[id];
    }

    inline LinkListView& getLinkList(InternalId id, uint32_t level) const {
        return *reinterpret_cast<LinkListView*>(level_data_[level] + id * getLinkListSize(level));
    }

};


template<typename T>
using SmallTopPQueue = std::priority_queue<T, std::vector<T>, std::greater<T>>;  

template<typename T>
using BigTopPQueue = std::priority_queue<T, std::vector<T>, std::less<T>>;

// SpaceT requirements (static duck-typing contract):
//   static dist_t distFunc(const dist_t* a, const dist_t* b, size_t dim)  — inlinable
//   size_t getDim() const
//   size_t getDataSize() const
template<typename dist_t, typename SpaceT>
class HierarchicalNSW : public AlgorithmInterface<dist_t> {
    // space_ must be declared before dim_/data_size_ so its ctor runs first
    // (C++ initializes members in declaration order, not initializer-list order)
    SpaceT space_; // held by value — compiler can inline SpaceT::distFunc

    // basic parameters
    const size_t elementSize_;
    const size_t dim_;
    const size_t data_size_;

    // hnsw specific parameters
    size_t M_;
    size_t efConstruction_;
    size_t efSearch_;
    size_t random_seed_;
    Layer0NeighborMode layer0_mode_;

    // data stores
    DataStoreAligned<dist_t, 64> point_store_;
    DataStore<LabelType> label_store_;

    // hnsw index structures
    LinkLists link_lists_;

    std::vector<size_t> point_order_;
    std::atomic<size_t> cur_element_count_;

    // Entry-point initialization barrier: the internal_id=0 node must have its
    // point data fully written before any other thread begins beam search.
    std::atomic<bool> entry_ready_;

    // Lock striping for neighbor lists — see kLockStripes at top of file.
    mutable std::array<std::mutex, kLockStripes> link_list_locks_;

    inline std::mutex& getLinkListMutex(InternalId id) const {
        return link_list_locks_[id & (kLockStripes - 1)];
    }

 public:
    HierarchicalNSW(SpaceT space, size_t elementSize,
        size_t M, size_t efConstruction, 
        size_t random_seed = 42,
        Layer0NeighborMode layer0_mode = Layer0NeighborMode::kDoubleM)
        : AlgorithmInterface<dist_t>(), space_(std::move(space)), elementSize_(elementSize),
          dim_(space_.getDim()), data_size_(space_.getDataSize()),
          M_(M), efConstruction_(efConstruction), efSearch_(efConstruction), random_seed_(random_seed),
          layer0_mode_(layer0_mode),
          point_store_(dim_, elementSize_), label_store_(1, elementSize_),
          link_lists_(M, elementSize_, layer0_mode), point_order_(elementSize_),
          cur_element_count_(0), entry_ready_(false) {
        
        // 填充point_order_，并打乱顺序
        std::iota(point_order_.begin(), point_order_.end(), 0);
        std::mt19937 rng(random_seed_);
        std::shuffle(point_order_.begin()+1, point_order_.end(), rng);
    }

    size_t getEfSearch() const override { return efSearch_; }
    void setEfSearch(size_t ef) override { efSearch_ = ef; }

    void addPoint(const void *data_point, LabelType label_type) override {
        // Atomically claim the next slot.  fetch_add returns the OLD value (idx),
        // so idx==0 means this thread is inserting the very first point.
        size_t idx = cur_element_count_.fetch_add(1, std::memory_order_acq_rel);
        if (idx >= elementSize_) {
            cur_element_count_.fetch_sub(1, std::memory_order_relaxed);
            throw std::runtime_error("Maximum number of elements reached");
        }

        InternalId internal_id = point_order_[idx];
        // Store the data point
        if (!point_store_.setData(internal_id, static_cast<const dist_t*>(data_point))) {
            throw std::runtime_error("Failed to store data point");
        }
        // Store the label
        if (!label_store_.setData(internal_id, &label_type)) {
            throw std::runtime_error("Failed to store label");
        }

        if (idx == 0) {
            // Signal that the entry point (internal_id=0) data is ready.
            entry_ready_.store(true, std::memory_order_release);
            return; // 第一个点不需要建立连接
        }

        // Ensure the entry point's data is fully written before beam search starts.
        while (!entry_ready_.load(std::memory_order_acquire)) { /* spin */ }

        updatePoint(data_point, internal_id);
    }


    void updatePoint(const void *data_point, InternalId internal_id) {
        InternalId enterpoint_id = 0; // always start from entry point 0

        // HNSW index update logic goes here
        // overhigh levels search
        auto nearest_id = enterpoint_id;
        auto nearest_dist = SpaceT::distFunc(static_cast<const dist_t*>(data_point), point_store_.getData(enterpoint_id), dim_);
        for (size_t level = link_lists_.getMaxLevel(); level > link_lists_.getLevel(internal_id); --level) {
            std::tie(nearest_dist, nearest_id) = searchNearestAtLevel(data_point, nearest_id, nearest_dist, level);
        }
        // from level of the new point down to level 0
        for (size_t level = link_lists_.getLevel(internal_id) + 1; level-- > 0;) {
            updateNewPointAtLevel(internal_id, nearest_id, level);
        }
    }

    void updateNewPointAtLevel(InternalId internal_id, InternalId enterpoint_id, size_t level) {
        // Use beam search (ef = efConstruction_) to find construction candidates,
        // mirroring the standard HNSW SEARCH-LAYER algorithm.
        dist_t ep_dist = SpaceT::distFunc(point_store_.getData(internal_id),
                                          point_store_.getData(enterpoint_id), dim_);

        SmallTopPQueue<std::pair<dist_t, InternalId>> searchCandidates;
        BigTopPQueue<std::pair<dist_t, InternalId>> candidates;
        VisitedTable visited_table(elementSize_);
        visited_table.mark(enterpoint_id);
        visited_table.mark(internal_id); // skip self

        searchCandidates.emplace(ep_dist, enterpoint_id);
        candidates.emplace(ep_dist, enterpoint_id);

        std::vector<InternalId> neighbors;
        neighbors.reserve(link_lists_.getM(level));
        while (!searchCandidates.empty()) {
            auto [c_dist, search_id] = searchCandidates.top();
            searchCandidates.pop();

            if (candidates.size() >= efConstruction_ && c_dist > candidates.top().first) {
                break;
            }

            // Snapshot neighbor list under lock, then compute distances lock-free.
            neighbors.clear();
            {
                std::unique_lock<std::mutex> lock(getLinkListMutex(search_id));
                const LinkListView& ll = link_lists_.getLinkList(search_id, level);
                neighbors.assign(ll.data, ll.data + ll.size);
            }
            for (InternalId cand_id : neighbors) {
                if (visited_table.isVisited(cand_id)) continue;
                visited_table.mark(cand_id);
                dist_t d = SpaceT::distFunc(point_store_.getData(internal_id),
                                            point_store_.getData(cand_id), dim_);
                if (candidates.size() < efConstruction_ || d < candidates.top().first) {
                    searchCandidates.emplace(d, cand_id);
                    candidates.emplace(d, cand_id);
                    if (candidates.size() > efConstruction_) {
                        candidates.pop();
                    }
                }
            }
        }

        const size_t M_cur = link_lists_.getM(level);
        
        // 根据模式和层级选择邻居
        if (level == 0 && layer0_mode_ == Layer0NeighborMode::kHeuristicPlusClosest) {
            // 模式3: M 个启发式 + M 个最近邻居
            selectHeuristicPlusClosest(candidates, M_cur / 2);
        } else {
            // 模式1, 模式2, 或其他层: 使用启发式选择
            getNeighborsByHeuristic2(candidates, M_cur);
        }

        // Write new node's forward links under its stripe lock.
        std::vector<InternalId> new_neighbors;
        {
            std::unique_lock<std::mutex> lock(getLinkListMutex(internal_id));
            LinkListView& link_list = link_lists_.getLinkList(internal_id, level);
            link_list.size = 0;
            while (!candidates.empty()) {
                auto candidate_id = candidates.top().second;
                link_list.data[link_list.size++] = candidate_id;
                new_neighbors.push_back(candidate_id);
                candidates.pop();
            }
        }
        // Add reverse edges; each existing node is locked individually.
        for (InternalId candidate_id : new_neighbors) {
            updateExistPointAtLevel(candidate_id, internal_id, level);
        }
    }

    void updateExistPointAtLevel(InternalId internal_id, InternalId new_id, size_t level) {
        // Update the neighbor list for an existing node that just gained a new candidate (new_id).
        // Fast-path (hnswlib parity): if the list is not yet full, just append.
        // Only run the expensive heuristic when the list is already at capacity.
        std::unique_lock<std::mutex> lock(getLinkListMutex(internal_id));
        LinkListView& link_list = link_lists_.getLinkList(internal_id, level);
        const size_t Mcurmax = link_lists_.getM(level);

        if (link_list.size < Mcurmax) {
            // List not full — plain append, no distance computation needed.
            link_list.data[link_list.size++] = new_id;
            return;
        }

        // List is full — collect current neighbors + new_id, then prune with heuristic.
        BigTopPQueue<std::pair<dist_t, InternalId>> candidates;
        dist_t d_new = SpaceT::distFunc(point_store_.getData(internal_id),
                                        point_store_.getData(new_id), dim_);
        candidates.emplace(d_new, new_id);
        for (size_t i = 0; i < link_list.size; ++i) {
            InternalId nb = link_list.data[i];
            dist_t d = SpaceT::distFunc(point_store_.getData(internal_id),
                                        point_store_.getData(nb), dim_);
            candidates.emplace(d, nb);
        }

        // 根据模式和层级选择邻居
        if (level == 0 && layer0_mode_ == Layer0NeighborMode::kHeuristicPlusClosest) {
            // 模式3: M 个启发式 + M 个最近邻居
            selectHeuristicPlusClosest(candidates, Mcurmax / 2);
        } else {
            // 模式1, 模式2, 或其他层: 使用启发式选择
            getNeighborsByHeuristic2(candidates, Mcurmax);
        }

        link_list.size = 0;
        while (!candidates.empty()) {
            link_list.data[link_list.size++] = candidates.top().second;
            candidates.pop();
        }
    }

    std::pair<dist_t, InternalId> 
    searchNearestAtLevel(const void* query_data, InternalId nearest_id, dist_t nearest_dist, size_t level, LevelSearchStats* stats = nullptr) const {
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
                if (stats) ++stats->dist_calcs;
                dist_t dist = SpaceT::distFunc(static_cast<const dist_t*>(query_data), point_store_.getData(candidate_id), dim_);
                if (dist < nearest_dist) {
                    nearest_dist = dist;
                    nearest_id = candidate_id;
                    changed = true;
                    if (stats) ++stats->hops;
                }
            }
        }
        return {nearest_dist, nearest_id};
    }

    std::vector<InternalId>
    getTwoHopNeighborsOnLevel(InternalId enterpoint_id, size_t level) const {
        std::set<InternalId> candidates;

        // add in one hop neighbors of enterpoint
        const auto& enterpoint_link_list = link_lists_.getLinkList(enterpoint_id, level);
        for (size_t i = 0; i < enterpoint_link_list.size; ++i) {
            InternalId candidate_id = enterpoint_link_list.data[i];
            candidates.insert(candidate_id);
        }

        // add in two hop neighbors of enterpoint
        for(size_t i = 0; i < enterpoint_link_list.size; ++i) {
            InternalId first_hop_id = enterpoint_link_list.data[i];
            const auto& first_hop_link_list = link_lists_.getLinkList(first_hop_id, level);
            for (size_t j = 0; j < first_hop_link_list.size; ++j) {
                InternalId candidate_id = first_hop_link_list.data[j];
                if (candidate_id == enterpoint_id) {
                    continue;
                }
                candidates.insert(candidate_id);
            }
        }
        
        return std::vector<InternalId>(candidates.begin(), candidates.end());
    }

    BigTopPQueue<std::pair<dist_t, InternalId>>
    getNeighborsByHeuristic2(BigTopPQueue<std::pair<dist_t, InternalId>> &top_candidates, const size_t M) {
        // Fast-path (hnswlib parity): when candidate count ≤ M all of them will
        // be selected anyway — skip the O(N²) inter-candidate distance work.
        if (top_candidates.size() <= M) {
            return top_candidates;
        }

        // 将候选者转移到vector中（大顶堆弹出顺序是从大到小）
        std::vector<std::pair<dist_t, InternalId>> candidates;
        candidates.reserve(top_candidates.size());
        while (!top_candidates.empty()) {
            candidates.push_back(top_candidates.top());
            top_candidates.pop();
        }
        // 反转使其按距离从小到大排列（最近的在前）
        std::reverse(candidates.begin(), candidates.end());

        // 启发式选择
        std::vector<std::pair<dist_t, InternalId>> return_list;
        for (const auto& current_pair : candidates) {
            if (return_list.size() >= M) {
                break;
            }
            dist_t dist_to_query = current_pair.first;
            bool good = true;

            for (const auto& second_pair : return_list) {
                dist_t curdist = SpaceT::distFunc(
                        point_store_.getData(second_pair.second),
                        point_store_.getData(current_pair.second),
                        dim_);
                if (curdist < dist_to_query) {
                    good = false;
                    break;
                }
            }
            if (good) {
                return_list.push_back(current_pair);
            }
        }

        // 将结果放回 top_candidates
        for (const auto& p : return_list) {
            top_candidates.emplace(p.first, p.second);
        }
        return top_candidates;
    }

    // 模式3专用：选择 M 个启发式邻居 + M 个最近邻居
    // candidates 是大顶堆（按距离），堆顶是距离最远的
    void selectHeuristicPlusClosest(
            BigTopPQueue<std::pair<dist_t, InternalId>>& candidates, size_t M) {
        // 1. 备份所有候选者
        std::vector<std::pair<dist_t, InternalId>> all_candidates;
        while (!candidates.empty()) {
            all_candidates.push_back(candidates.top());
            candidates.pop();
        }
        
        // 2. 恢复候选者用于启发式选择
        BigTopPQueue<std::pair<dist_t, InternalId>> temp_queue;
        for (const auto& p : all_candidates) {
            temp_queue.push(p);
        }
        
        // 3. 启发式选择 M 个邻居
        getNeighborsByHeuristic2(temp_queue, M);
        
        // 4. 收集启发式选中的 ID
        std::set<InternalId> heuristic_ids;
        std::vector<std::pair<dist_t, InternalId>> heuristic_results;
        while (!temp_queue.empty()) {
            heuristic_ids.insert(temp_queue.top().second);
            heuristic_results.push_back(temp_queue.top());
            temp_queue.pop();
        }
        
        // 5. 从剩余候选者中选择最近的 M 个
        // all_candidates 按距离从大到小排列，需要重新排序
        std::vector<std::pair<dist_t, InternalId>> remaining;
        for (const auto& p : all_candidates) {
            if (heuristic_ids.find(p.second) == heuristic_ids.end()) {
                remaining.push_back(p);  // p.first 是距离（大顶堆中是大的在前）
            }
        }
        // remaining 已经按距离从大到小排好，取后 M 个（即最小的 M 个）
        // 或者从小到大取前 M 个
        std::vector<std::pair<dist_t, InternalId>> closest_results;
        size_t closest_count = std::min(M, remaining.size());
        // 从 remaining 末尾取（距离最小的）
        for (size_t i = 0; i < closest_count; ++i) {
            size_t idx = remaining.size() - 1 - i;
            closest_results.push_back(remaining[idx]);
        }
        
        // 6. 合并结果放入 candidates（先放启发式的，再放最近的）
        for (const auto& p : heuristic_results) {
            candidates.push(p);
        }
        for (const auto& p : closest_results) {
            candidates.push(p);
        }
    }


    void print(){
        for (size_t level = link_lists_.getMaxLevel(); level-- > 0; ) {
            printf("Level %zu:\n", level);
            for (InternalId id = 0; id < cur_element_count_; ++id) {
                const LinkListView& link_list = link_lists_.getLinkList(id, level);
                printf("  Node %u, size:%u, list: ", id, link_list.size);
                for (size_t i = 0; i < link_list.size; ++i) {
                    printf("%u ", link_list.data[i]);
                }
                printf("\n");
            }
        }
    }
    

    BigTopPQueue<std::pair<dist_t, LabelType>>
    searchKnn(const void* query_data, size_t k) const override {
        InternalId enterpoint_id = 0; // always start from entry point 0

        // HNSW index update logic goes here
        // overhigh levels search
        auto nearest_id = enterpoint_id;
        auto nearest_dist = SpaceT::distFunc(static_cast<const dist_t*>(query_data), point_store_.getData(enterpoint_id), dim_);
        // Greedy descent from maxLevel down to level 1; level 0 handled by beam search below.
        for (size_t level = link_lists_.getMaxLevel() + 1; level-- > 1; ) {
            std::tie(nearest_dist, nearest_id) = searchNearestAtLevel(query_data, nearest_id, nearest_dist, level);
        }

        auto topCandidates = searchKnnAtBaseLevel(query_data, nearest_id, nearest_dist, k);

        // Trim to exactly k results
        while (topCandidates.size() > k) {
            topCandidates.pop();
        }

        // Prepare final result with labels
        BigTopPQueue<std::pair<dist_t, LabelType>> result;
        while (!topCandidates.empty()) {
            auto [dist, internal_id] = topCandidates.top();
            topCandidates.pop();
            result.emplace(dist, *label_store_.getData(internal_id));
        }
        return result;
    }

    BigTopPQueue<std::pair<dist_t, InternalId>>
    searchKnnAtBaseLevel(const void* query_data, InternalId enterpoint_id, dist_t enterpoint_dist, size_t k, SearchDebugStats* stats = nullptr) const {
        auto level = 0;
        auto ef = std::max(k, efSearch_);

        SmallTopPQueue<std::pair<dist_t, InternalId>> searchCandidates;
        BigTopPQueue<std::pair<dist_t, InternalId>> topCandidates;
        VisitedTable visited_table(elementSize_);
        visited_table.mark(enterpoint_id);

        searchCandidates.emplace(enterpoint_dist, enterpoint_id);
        topCandidates.emplace(enterpoint_dist, enterpoint_id);

        if (stats) {
          stats->level0_entry_point = enterpoint_id;
          stats->visited_in_level0.push_back(enterpoint_id);
          ++stats->visited_nodes;
        }

        while (!searchCandidates.empty()) {
            auto [c_dist, search_id] = searchCandidates.top();
            searchCandidates.pop();

            // Standard HNSW early stop: closest unvisited candidate is farther
            // than worst in topCandidates and we already have ef results.
            if (topCandidates.size() >= ef && c_dist > topCandidates.top().first) {
                break;
            }

            const LinkListView& link_list = link_lists_.getLinkList(search_id, level);
            for (size_t i = 0; i < link_list.size; ++i) {
                InternalId candidate_id = link_list.data[i];
                if (visited_table.isVisited(candidate_id)) {
                    continue;
                }
                visited_table.mark(candidate_id);
                if (stats) {
                  ++stats->base_level.dist_calcs;
                  stats->visited_in_level0.push_back(candidate_id);
                  ++stats->visited_nodes;
                }
                dist_t dist = SpaceT::distFunc(static_cast<const dist_t*>(query_data), point_store_.getData(candidate_id), dim_);
                if (topCandidates.size() < ef || dist < topCandidates.top().first) {
                    searchCandidates.emplace(dist, candidate_id);
                    topCandidates.emplace(dist, candidate_id);
                    if (topCandidates.size() > ef) {
                        topCandidates.pop();
                    }
                }
            }
        }
        return topCandidates;
    }

    // =========================================================================
    // 带调试统计的搜索接口
    // =========================================================================

    std::pair<BigTopPQueue<std::pair<dist_t, LabelType>>, SearchDebugStats>
    searchKnnWithStats(const void* query_data, size_t k) const {
        SearchDebugStats stats;
        InternalId enterpoint_id = 0;

        auto nearest_id = enterpoint_id;
        auto nearest_dist = SpaceT::distFunc(static_cast<const dist_t*>(query_data), 
                                             point_store_.getData(enterpoint_id), dim_);

        // 高层搜索（level >= 1）
        stats.level_stats.resize(link_lists_.getMaxLevel() + 1);
        for (size_t level = link_lists_.getMaxLevel() + 1; level-- > 1; ) {
            LevelSearchStats level_stat;
            std::tie(nearest_dist, nearest_id) = searchNearestAtLevel(
                query_data, nearest_id, nearest_dist, level, &level_stat);
            stats.level_stats[level] = level_stat;

            // 更新高层总计
            stats.high_level_total.dist_calcs += level_stat.dist_calcs;
            stats.high_level_total.hops += level_stat.hops;
            ++stats.high_level_layers;

            // 更新最大值
            if (level_stat.dist_calcs > stats.high_level_dist_max_count) {
                stats.high_level_dist_max_count = level_stat.dist_calcs;
                stats.high_level_dist_max_layer = level;
            }
            if (level_stat.hops > stats.high_level_hops_max_count) {
                stats.high_level_hops_max_count = level_stat.hops;
                stats.high_level_hops_max_layer = level;
            }
        }

        // 底层搜索（level 0）
        auto topCandidates = searchKnnAtBaseLevel(query_data, nearest_id, nearest_dist, k, &stats);

        // 计算各访问节点的跳数（BFS）
        if (!stats.visited_in_level0.empty()) {
            auto hops_map = computeHopsFromEntry(stats.level0_entry_point, 0);
            for (InternalId visited_id : stats.visited_in_level0) {
                auto it = hops_map.find(visited_id);
                if (it != hops_map.end() && it->second > stats.max_hops_from_entry) {
                    stats.max_hops_from_entry = it->second;
                }
            }
        }

        // Trim to exactly k results
        while (topCandidates.size() > k) {
            topCandidates.pop();
        }

        // Prepare final result with labels
        BigTopPQueue<std::pair<dist_t, LabelType>> result;
        while (!topCandidates.empty()) {
            auto [dist, internal_id] = topCandidates.top();
            topCandidates.pop();
            result.emplace(dist, *label_store_.getData(internal_id));
        }
        return {result, stats};
    }

    // =========================================================================
    // 缺失点分析
    // =========================================================================

    MissedPointAnalysis analyzeMissedPoint(InternalId missed_id,
                                           const SearchDebugStats& stats,
                                           double query_dist) const {
        MissedPointAnalysis result;
        result.point_id = missed_id;
        result.dist = query_dist;

        // 检查是否被访问过
        auto it = std::find(stats.visited_in_level0.begin(),
                            stats.visited_in_level0.end(), missed_id);
        result.was_visited = (it != stats.visited_in_level0.end());

        // BFS 计算跳数
        auto hops_map = computeHopsFromEntry(stats.level0_entry_point, 0);
        auto hop_it = hops_map.find(missed_id);
        if (hop_it != hops_map.end()) {
            result.hops_from_entry = hop_it->second;
            result.reachable = true;
        } else {
            result.hops_from_entry = std::numeric_limits<size_t>::max();
            result.reachable = false;
        }

        // 如果未被访问，计算最小 ef
        if (!result.was_visited && result.reachable) {
            result.min_ef_to_reach = computeMinEfToReach(
                missed_id, static_cast<dist_t>(query_dist), stats.level0_entry_point);
        }

        return result;
    }

    std::vector<MissedPointAnalysis>
    analyzeMissedPoints(const std::vector<InternalId>& missed_ids,
                        const SearchDebugStats& stats,
                        const void* query_data) const {
        std::vector<MissedPointAnalysis> results;
        results.reserve(missed_ids.size());

        // 预计算跳数
        auto hops_map = computeHopsFromEntry(stats.level0_entry_point, 0);

        for (InternalId missed_id : missed_ids) {
            MissedPointAnalysis result;
            result.point_id = missed_id;
            result.dist = static_cast<double>(SpaceT::distFunc(
                static_cast<const dist_t*>(query_data),
                point_store_.getData(missed_id), dim_));

            // 检查是否被访问过
            auto it = std::find(stats.visited_in_level0.begin(),
                                stats.visited_in_level0.end(), missed_id);
            result.was_visited = (it != stats.visited_in_level0.end());

            // 跳数
            auto hop_it = hops_map.find(missed_id);
            if (hop_it != hops_map.end()) {
                result.hops_from_entry = hop_it->second;
                result.reachable = true;
            } else {
                result.hops_from_entry = std::numeric_limits<size_t>::max();
                result.reachable = false;
            }

            // 最小 ef
            if (!result.was_visited && result.reachable) {
                result.min_ef_to_reach = computeMinEfToReach(
                    missed_id, static_cast<dist_t>(result.dist), stats.level0_entry_point);
            }

            results.push_back(result);
        }
        return results;
    }

    // =========================================================================
    // 连通性分析
    // =========================================================================

    ConnectivityReport analyzeConnectivity() const {
        ConnectivityReport report;

        // 从入口节点(id=0)出发的各层连通性
        for (size_t level = 0; level <= link_lists_.getMaxLevel(); ++level) {
            report.from_entry.push_back(analyzeLevelConnectivity({0}, level));
        }

        // 从上层节点出发的各层连通性
        for (size_t level = 0; level <= link_lists_.getMaxLevel(); ++level) {
            if (level == link_lists_.getMaxLevel()) {
                // 最高层没有上层，用入口节点
                report.from_upper_layer.push_back(analyzeLevelConnectivity({0}, level));
            } else {
                // 收集 level+1 层所有节点作为入口
                std::vector<InternalId> upper_nodes = getNodesAtLevel(level + 1);
                if (upper_nodes.empty()) {
                    upper_nodes.push_back(0);  // 回退到入口节点
                }
                report.from_upper_layer.push_back(
                    analyzeLevelConnectivity(upper_nodes, level));
            }
        }

        return report;
    }

 private:
    // 从指定入口点出发，用 BFS 计算到各节点的跳数
    std::unordered_map<InternalId, size_t>
    computeHopsFromEntry(InternalId entry_id, size_t level) const {
        std::unordered_map<InternalId, size_t> hops;
        if (cur_element_count_.load(std::memory_order_relaxed) == 0) {
            return hops;
        }

        std::queue<std::pair<InternalId, size_t>> bfs_queue;
        VisitedTable visited(elementSize_);

        bfs_queue.push({entry_id, 0});
        visited.mark(entry_id);
        hops[entry_id] = 0;

        while (!bfs_queue.empty()) {
            auto [current_id, current_hops] = bfs_queue.front();
            bfs_queue.pop();

            // 获取邻居（需要加锁）
            std::vector<InternalId> neighbors;
            {
                std::unique_lock<std::mutex> lock(getLinkListMutex(current_id));
                const LinkListView& ll = link_lists_.getLinkList(current_id, level);
                neighbors.assign(ll.data, ll.data + ll.size);
            }

            for (InternalId neighbor_id : neighbors) {
                if (!visited.isVisited(neighbor_id)) {
                    visited.mark(neighbor_id);
                    hops[neighbor_id] = current_hops + 1;
                    bfs_queue.push({neighbor_id, current_hops + 1});
                }
            }
        }
        return hops;
    }

    // 计算找到指定点需要的最小 ef
    size_t computeMinEfToReach(InternalId target_id, dist_t target_dist,
                               InternalId entry_id) const {
        // 使用二分搜索找到最小 ef
        size_t low = 1;
        size_t high = elementSize_;
        size_t result = std::numeric_limits<size_t>::max();

        while (low <= high) {
            size_t mid = low + (high - low) / 2;
            if (canReachWithEf(target_id, target_dist, entry_id, mid)) {
                result = mid;
                high = mid - 1;
            } else {
                low = mid + 1;
            }
        }
        return result;
    }

    // 检查指定 ef 是否能找到目标点
    bool canReachWithEf(InternalId target_id, dist_t target_dist,
                        InternalId entry_id, size_t ef) const {
        // 模拟 beam search
        dist_t entry_dist = SpaceT::distFunc(point_store_.getData(entry_id),
                                             point_store_.getData(target_id), dim_);

        SmallTopPQueue<std::pair<dist_t, InternalId>> searchCandidates;
        BigTopPQueue<std::pair<dist_t, InternalId>> topCandidates;
        VisitedTable visited(elementSize_);
        visited.mark(entry_id);

        searchCandidates.emplace(entry_dist, entry_id);
        topCandidates.emplace(entry_dist, entry_id);

        while (!searchCandidates.empty()) {
            auto [c_dist, search_id] = searchCandidates.top();
            searchCandidates.pop();

            if (topCandidates.size() >= ef && c_dist > topCandidates.top().first) {
                break;
            }

            const LinkListView& link_list = link_lists_.getLinkList(search_id, 0);
            for (size_t i = 0; i < link_list.size; ++i) {
                InternalId candidate_id = link_list.data[i];
                if (candidate_id == target_id) {
                    return true;  // 找到目标点
                }
                if (visited.isVisited(candidate_id)) {
                    continue;
                }
                visited.mark(candidate_id);
                dist_t dist = SpaceT::distFunc(point_store_.getData(target_id),
                                               point_store_.getData(candidate_id), dim_);
                if (topCandidates.size() < ef || dist < topCandidates.top().first) {
                    searchCandidates.emplace(dist, candidate_id);
                    topCandidates.emplace(dist, candidate_id);
                    if (topCandidates.size() > ef) {
                        topCandidates.pop();
                    }
                }
            }
        }
        return false;
    }

    // 获取指定层的所有节点
    std::vector<InternalId> getNodesAtLevel(size_t level) const {
        std::vector<InternalId> nodes;
        size_t count = cur_element_count_.load(std::memory_order_relaxed);
        for (InternalId id = 0; id < count; ++id) {
            if (link_lists_.getLevel(id) >= level) {
                nodes.push_back(id);
            }
        }
        return nodes;
    }

    // 分析从指定入口节点集合出发的层连通性
    LevelConnectivityStats analyzeLevelConnectivity(
        const std::vector<InternalId>& entry_ids, size_t level) const {
        LevelConnectivityStats stats;
        stats.level = level;

        // 计算该层总节点数
        size_t count = cur_element_count_.load(std::memory_order_relaxed);
        for (InternalId id = 0; id < count; ++id) {
            if (link_lists_.getLevel(id) >= level) {
                ++stats.total_nodes;
            }
        }

        if (entry_ids.empty()) {
            stats.unreachable_nodes = stats.total_nodes;
            return stats;
        }

        // BFS 遍历
        VisitedTable visited(elementSize_);
        std::queue<InternalId> bfs_queue;

        for (InternalId entry_id : entry_ids) {
            if (link_lists_.getLevel(entry_id) >= level) {
                bfs_queue.push(entry_id);
                visited.mark(entry_id);
            }
        }

        while (!bfs_queue.empty()) {
            InternalId current_id = bfs_queue.front();
            bfs_queue.pop();

            std::vector<InternalId> neighbors;
            {
                std::unique_lock<std::mutex> lock(getLinkListMutex(current_id));
                const LinkListView& ll = link_lists_.getLinkList(current_id, level);
                neighbors.assign(ll.data, ll.data + ll.size);
            }

            for (InternalId neighbor_id : neighbors) {
                if (!visited.isVisited(neighbor_id)) {
                    visited.mark(neighbor_id);
                    bfs_queue.push(neighbor_id);
                }
            }
        }

        // 统计可达和不可达节点
        for (InternalId id = 0; id < count; ++id) {
            if (link_lists_.getLevel(id) >= level) {
                if (visited.isVisited(id)) {
                    ++stats.reachable_nodes;
                } else {
                    ++stats.unreachable_nodes;
                    stats.unreachable_ids.push_back(id);
                }
            }
        }

        return stats;
    }
};


} // namespace arena_hnswlib
