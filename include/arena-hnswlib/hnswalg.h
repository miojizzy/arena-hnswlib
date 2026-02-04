#pragma once

#include <cstddef>
#include "arena_hnswlib.h"
#include "data_store.h"
#include <random>

namespace arena_hnswlib {


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
            auto num = static_cast<size_t>(std::ceil(level_sizes.back() * 1.0 / M_));
            level_sizes.emplace_back(num);
        }
        maxLevel_ = level_sizes.size() - 1;

        // 分配 internal id 到 level 的映射
        internal_id_level_.resize(elementSize_, 0);
        auto level = maxLevel_;
        for(auto i = 0; i < elementSize_; ++i) {
            if (i < level_sizes[level - 1] && level > 0) {
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

template<typename dist_t>
class HierarchicalNSW :public AlgorithmInterface<dist_t> {
    // basic parameters
    const size_t elementSize_;
    const size_t dim_;
    const size_t data_size_;
    const DISTFUNC<dist_t> dist_func_;

    // hnsw specific parameters
    size_t M_;
    size_t efConstruction_;
    size_t random_seed_;

    // data stores
    DataStoreAligned<dist_t, 64> point_store_;
    DataStore<LabelType> label_store_;

    // hnsw index structures
    LinkLists link_lists_;

    std::vector<size_t> point_order_;
    size_t cur_element_count_;

public:
    HierarchicalNSW(SpacePtr<dist_t> s, size_t elementSize, 
        size_t M, size_t efConstruction, size_t random_seed=42) 
        : AlgorithmInterface<dist_t>(std::move(s)), elementSize_(elementSize),
          dim_(this->space_->getDim()), data_size_(this->space_->getDataSize()),
          dist_func_(this->space_->getDistFunc()),
          M_(M), efConstruction_(efConstruction), random_seed_(random_seed),
          point_store_(dim_, elementSize_), label_store_(1, elementSize_),
          link_lists_(M, elementSize_), point_order_(elementSize_), cur_element_count_(0) {
        
        // 填充point_order_，并打乱顺序
        std::iota(point_order_.begin(), point_order_.end(), 0);
        std::mt19937 rng(random_seed_);
        std::shuffle(point_order_.begin()+1, point_order_.end(), rng);
    }


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
        auto nearest_dist = dist_func_(data_point, point_store_.getData(enterpoint_id), &dim_);
        for (size_t level = link_lists_.getMaxLevel(); level > link_lists_.getLevel(internal_id); --level) {
            std::tie(nearest_dist, nearest_id) = searchNearestAtLevel(data_point, nearest_id, nearest_dist, level);
        }
        // from level of the new point down to level 0
        for (size_t level = link_lists_.getLevel(internal_id) + 1; level-- > 0;) {
            updateNewPointAtLevel(internal_id, nearest_id, level);
        }
    }

    void updateNewPointAtLevel(InternalId internal_id, InternalId enterpoint_id, size_t level) {
        // printf("Updating new point %u at level %zu with entry point %u\n", internal_id, level, enterpoint_id);
        auto all_neighbors = getTwoHopNeighborsOnLevel(enterpoint_id, level);
        all_neighbors.emplace_back(enterpoint_id);

        auto candidates = std::priority_queue<std::pair<dist_t, InternalId>>();
        for(auto cand: all_neighbors) {
            dist_t dist = dist_func_(point_store_.getData(internal_id), point_store_.getData(cand), &dim_);
            candidates.push({dist, cand});
        }
        while (candidates.size() > efConstruction_) {
            candidates.pop();
        }

        getNeighborsByHeuristic2(candidates, link_lists_.getM(level));

        LinkListView& link_list = link_lists_.getLinkList(internal_id, level);
        link_list.size = 0;
        while (!candidates.empty()) {
            auto candidate_id = candidates.top().second;
            // printf("  Adding neighbor %u to point %u at level %zu\n", candidate_id, internal_id, level);
            link_list.data[link_list.size++] = candidate_id;
            candidates.pop();
        }
        for (size_t i = 0; i < link_list.size; ++i) {
            InternalId candidate_id = link_list.data[i];
            updateExistPointAtLevel(candidate_id, internal_id, level);
        }
    }

    void updateExistPointAtLevel(InternalId internal_id, InternalId new_id, size_t level) {
        // printf("Updating existing point %u at level %zu with new point %u\n", internal_id, level, new_id);
        auto all_neighbors = getTwoHopNeighborsOnLevel(internal_id, level);
        all_neighbors.emplace_back(new_id);
        auto candidates = std::priority_queue<std::pair<dist_t, InternalId>>();
        for(auto cand: all_neighbors) {
            dist_t dist = dist_func_(point_store_.getData(internal_id), point_store_.getData(cand), &dim_);
            candidates.push({dist, cand});
        }
        while (candidates.size() > efConstruction_) {
            candidates.pop();
        }

        getNeighborsByHeuristic2(candidates, link_lists_.getM(level));

        LinkListView& link_list = link_lists_.getLinkList(internal_id, level);
        link_list.size = 0;
        while (!candidates.empty()) {
            auto candidate_id = candidates.top().second;
            // printf("  Adding neighbor %u to point %u at level %zu\n", candidate_id, internal_id, level);
            link_list.data[link_list.size++] = candidate_id;
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
                dist_t dist = dist_func_(query_data, point_store_.getData(candidate_id), &dim_);
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
                dist_t curdist = dist_func_(
                        point_store_.getData(second_pair.second),
                        point_store_.getData(curent_pair.second),
                        &dim_);
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
        auto nearest_dist = dist_func_(query_data, point_store_.getData(enterpoint_id), &dim_);
        for (size_t level = link_lists_.getMaxLevel() + 1; level-- > 0; ) {
            std::tie(nearest_dist, nearest_id) = searchNearestAtLevel(query_data, nearest_id, nearest_dist, level);
        }

        auto topCandidates = searchKnnAtBaseLevel(query_data, nearest_id, nearest_dist, k);

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
        auto ef = std::max(k, efConstruction_);

        SmallTopPQueue<std::pair<dist_t, InternalId>> searchCandidates;
        BigTopPQueue<std::pair<dist_t, InternalId>> topCandidates;
        std::set<InternalId> visited;

        searchCandidates.emplace(enterpoint_dist, enterpoint_id);
        topCandidates.emplace(enterpoint_dist, enterpoint_id);
        visited.insert(enterpoint_id);

        while(topCandidates.size() < ef) {
            auto search_id = searchCandidates.top().second;
            searchCandidates.pop();
            const LinkListView& link_list = link_lists_.getLinkList(search_id, level);
            for (size_t i = 0; i < link_list.size; ++i) {
                InternalId candidate_id = link_list.data[i];
                if (visited.find(candidate_id) != visited.end()) {
                    continue;
                }
                visited.insert(candidate_id);
                dist_t dist = dist_func_(query_data, point_store_.getData(candidate_id), &dim_);
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