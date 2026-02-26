#pragma once

#include <cstddef>
#include "arena_hnswlib.h"
#include "data_store.h"
#include <random>
#include <stdexcept>
#include <numeric>
#include <set>

namespace arena_hnswlib {


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

 public:
    LinkLists(size_t M, size_t elementSize) 
        : M_(M), M0_(2*M), elementSize_(elementSize){
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

    // data stores
    DataStoreAligned<dist_t, 64> point_store_;
    DataStore<LabelType> label_store_;

    // hnsw index structures
    LinkLists link_lists_;

    std::vector<size_t> point_order_;
    size_t cur_element_count_;

 public:
    HierarchicalNSW(SpaceT space, size_t elementSize,
        size_t M, size_t efConstruction, size_t random_seed=42)
        : AlgorithmInterface<dist_t>(), space_(std::move(space)), elementSize_(elementSize),
          dim_(space_.getDim()), data_size_(space_.getDataSize()),
          M_(M), efConstruction_(efConstruction), efSearch_(efConstruction), random_seed_(random_seed),
          point_store_(dim_, elementSize_), label_store_(1, elementSize_),
          link_lists_(M, elementSize_), point_order_(elementSize_), cur_element_count_(0) {
        
        // 填充point_order_，并打乱顺序
        std::iota(point_order_.begin(), point_order_.end(), 0);
        std::mt19937 rng(random_seed_);
        std::shuffle(point_order_.begin()+1, point_order_.end(), rng);
    }

    size_t getEfSearch() const override { return efSearch_; }
    void setEfSearch(size_t ef) override { efSearch_ = ef; }

    void addPoint(const void *data_point, LabelType label_type) override {
        if (cur_element_count_ >= elementSize_) {
            throw std::runtime_error("Maximum number of elements reached");
        }

        InternalId internal_id = point_order_[cur_element_count_++];
        // Store the data point
        if (!point_store_.setData(internal_id, static_cast<const dist_t*>(data_point))) {
            throw std::runtime_error("Failed to store data point");
        }
        // Store the label
        if (!label_store_.setData(internal_id, &label_type)) {
            throw std::runtime_error("Failed to store label");
        }

        // printf("Adding point %u with label %u at internal id %u\n", cur_element_count_-1, label_type, internal_id);

        if (cur_element_count_ == 1) {
            return; // 第一个点不需要建立连接
        }

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

        while (!searchCandidates.empty()) {
            auto [c_dist, search_id] = searchCandidates.top();
            searchCandidates.pop();

            if (candidates.size() >= efConstruction_ && c_dist > candidates.top().first) {
                break;
            }

            const LinkListView& ll = link_lists_.getLinkList(search_id, level);
            for (size_t i = 0; i < ll.size; ++i) {
                InternalId cand_id = ll.data[i];
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

        getNeighborsByHeuristic2(candidates, link_lists_.getM(level));

        LinkListView& link_list = link_lists_.getLinkList(internal_id, level);
        link_list.size = 0;
        while (!candidates.empty()) {
            auto candidate_id = candidates.top().second;
            link_list.data[link_list.size++] = candidate_id;
            candidates.pop();
        }
        for (size_t i = 0; i < link_list.size; ++i) {
            InternalId candidate_id = link_list.data[i];
            updateExistPointAtLevel(candidate_id, internal_id, level);
        }
    }

    void updateExistPointAtLevel(InternalId internal_id, InternalId new_id, size_t level) {
        // Update the neighbor list for an existing node that just gained a new candidate (new_id).
        // Fast-path (hnswlib parity): if the list is not yet full, just append.
        // Only run the expensive heuristic when the list is already at capacity.
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

        getNeighborsByHeuristic2(candidates, Mcurmax);

        link_list.size = 0;
        while (!candidates.empty()) {
            link_list.data[link_list.size++] = candidates.top().second;
            candidates.pop();
        }
    }

    std::pair<dist_t, InternalId> 
    searchNearestAtLevel(const void* query_data, InternalId nearest_id, dist_t nearest_dist, size_t level) const {
        auto changed = true;
        while (changed) {
            changed = false;
            const LinkListView& link_list = link_lists_.getLinkList(nearest_id, level);
            for (size_t i = 0; i < link_list.size; ++i) {
                InternalId candidate_id = link_list.data[i];
                dist_t dist = SpaceT::distFunc(static_cast<const dist_t*>(query_data), point_store_.getData(candidate_id), dim_);
                if (dist < nearest_dist) {
                    nearest_dist = dist;
                    nearest_id = candidate_id;
                    changed = true;
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
        BigTopPQueue<std::pair<dist_t, InternalId>> queue_closest;
        std::vector<std::pair<dist_t, InternalId>> return_list;
        while (top_candidates.size() > 0) {
            queue_closest.emplace(-top_candidates.top().first, top_candidates.top().second);
            top_candidates.pop();
        }

        while (queue_closest.size()) {
            if (return_list.size() >= M)
                break;
            std::pair<dist_t, InternalId> curent_pair = queue_closest.top();
            dist_t dist_to_query = -curent_pair.first;
            queue_closest.pop();
            bool good = true;

            for (std::pair<dist_t, InternalId> second_pair : return_list) {
                dist_t curdist = SpaceT::distFunc(
                        point_store_.getData(second_pair.second),
                        point_store_.getData(curent_pair.second),
                        dim_);
                if (curdist < dist_to_query) {
                    good = false;
                    break;
                }
            }
            if (good) {
                return_list.push_back(curent_pair);
            }
        }

        for (std::pair<dist_t, InternalId> curent_pair : return_list) {
            top_candidates.emplace(-curent_pair.first, curent_pair.second);
        }
        // 返回结果队列（如需用）
        return top_candidates;
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
    searchKnnAtBaseLevel(const void* query_data, InternalId enterpoint_id, dist_t enterpoint_dist, size_t k) const {
        auto level = 0;
        auto ef = std::max(k, efSearch_);

        SmallTopPQueue<std::pair<dist_t, InternalId>> searchCandidates;
        BigTopPQueue<std::pair<dist_t, InternalId>> topCandidates;
        VisitedTable visited_table(elementSize_);
        visited_table.mark(enterpoint_id);

        searchCandidates.emplace(enterpoint_dist, enterpoint_id);
        topCandidates.emplace(enterpoint_dist, enterpoint_id);

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
};


} // namespace arena_hnswlib
