
#include <cstddef>
#include <iostream>
#include <limits>
#include <list>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

class KV_Block {
public:
  int init_ts;
  int last_accessed;
  std::size_t access_count;
  std::size_t sequence_depth;
  int parent_id;
  std::optional<std::list<int>::iterator> list_iterator;
};

class LRU_Store {
public:
  std::unordered_map<int, KV_Block> Store;
  std::size_t miss = 0, hit = 0;
  std::size_t cache_size;
  std::list<int> LRU_list;
  LRU_Store(std::size_t size) : Store(size), cache_size(size) {}
  LRU_Store(LRU_Store &) = delete;
  LRU_Store(LRU_Store &&) = delete;

  std::vector<std::pair<int, KV_Block>> evict(std::size_t N) {
    std::vector<std::pair<int, KV_Block>> result;
    while ((!LRU_list.empty() && result.size() < N)) {
      auto it = LRU_list.back();
      LRU_list.pop_back();
      result.insert(result.end(), 1,
                    std::pair<int, KV_Block>(it, Store.at(it)));
      Store.erase(it);
    }
    // for (auto &[id, block] : result) {
    //   std::cout << std::format("evicted id: {} init: {} last access: {} "
    //                            "sequence depth: {} prev: {} \n",
    //                            id, block.init_ts, block.last_accessed,
    //                            block.sequence_depth, block.parent_id);
    // }
    return result;
  }

  void on_access(int id, KV_Block &block) {
    if (block.list_iterator.has_value()) {
      LRU_list.erase(block.list_iterator.value());
    }
    LRU_list.push_front(id);
    block.list_iterator = LRU_list.begin();
  }

  void insert_or_update(int id, int ts, std::size_t sequence_depth,
                        int parent_id = -1) {
    auto it = Store.find(id);
    if (it != Store.end()) {
      hit++;
      auto *block = &it->second;
      block->access_count++;
      block->last_accessed = ts;
    } else {
      miss++;
      if (Store.size() == cache_size)
        evict(1);
      it = Store
               .insert(std::pair<int, KV_Block>(
                   id, {ts, ts, 1, sequence_depth, parent_id}))
               .first;
    }
    on_access(id, it->second);
  }
  ~LRU_Store() {}
};

class LFU_Block {
public:
  int init_ts;
  int last_accessed;
  std::size_t access_count;
  std::size_t sequence_depth;
  int parent_id;
  std::list<std::size_t>::iterator list_it;
};

class LFU_Store {
public:
  std::unordered_map<int, LFU_Block> Store;
  std::size_t miss = 0, hit = 0;
  std::unordered_map<int, std::list<std::size_t>> Freq_map;
  int min_freq = 1;
  std::size_t cache_size;
  LFU_Store(std::size_t cache_size)
      : cache_size(cache_size), Store(cache_size) {}
  std::vector<std::pair<int, LFU_Block>> evict(std::size_t N) {
    std::vector<std::pair<int, LFU_Block>> result;
    while (!Store.empty() && result.size() < N) {
      auto it = Freq_map.find(min_freq);
      if (it == Freq_map.end() || it->second.empty()) {
        min_freq++;
        continue;
      }

      auto &curr_freq = it->second;
      while (!curr_freq.empty() && result.size() < N) {
        int id = curr_freq.back();
        result.push_back(std::pair<int, LFU_Block>(id, Store.at(id)));
        Store.erase(id);
        curr_freq.pop_back();
      }
      if (curr_freq.empty())
        min_freq++;
    }
    // for (auto &[id, block] : result) {
    //   std::cout << std::format("evicted id: {} frequency: {} last access: {}
    //   "
    //                            "sequence depth: {} prev: {} \n",
    //                            id, block.access_count, block.last_accessed,
    //                            block.sequence_depth, block.parent_id);
    // }
    return result;
  }
  // void on_access(int id, LFU_Block &block) {}
  void insert_or_update(int id, int ts, std::size_t sequence_depth,
                        int parent_id = -1) {
    auto it = Store.find(id);
    if (it != Store.end()) {
      hit++;
      auto *block = &it->second;
      Freq_map.at(block->access_count).erase(block->list_it);
      if (block->access_count == min_freq && Freq_map.at(min_freq).empty()) {
        min_freq++;
      }
      block->access_count++;
      block->last_accessed = ts;

      if (!Freq_map.contains(block->access_count))
        Freq_map.insert(std::pair<int, std::list<std::size_t>>(
            block->access_count, std::list<std::size_t>()));

      auto &next_freq = Freq_map.at(block->access_count);
      block->list_it = next_freq.insert(next_freq.begin(), id);
    } else {
      miss++;
      if (Store.size() == cache_size)
        evict(1);
      min_freq = 1;
      if (!Freq_map.contains(1))
        Freq_map.insert(std::pair<int, std::list<std::size_t>>(
            1, std::list<std::size_t>()));
      auto &next_freq = Freq_map.at(1);
      auto it = next_freq.insert(next_freq.begin(), id);
      Store.insert(std::pair<int, LFU_Block>(
          id, {ts, ts, 1, sequence_depth, parent_id, it}));
    }
  }
};

class LengthAware_Store {
public:
  std::unordered_map<int, LFU_Block> Store;
  std::size_t miss = 0, hit = 0;
  std::unordered_map<int, std::list<std::size_t>> SequenceDepth_Map;
  int max_depth = 0;
  std::size_t cache_size;
  LengthAware_Store(std::size_t cache_size)
      : cache_size(cache_size), Store(cache_size) {}

  // evicts N or Store.size() (which ever is bigger) evicting only leaf nodes.
  // returns: eviction list (id,details)
  std::vector<std::pair<int, LFU_Block>> evict(std::size_t N) {
    std::vector<std::pair<int, LFU_Block>> result;
    while (!Store.empty() && result.size() < N) {
      auto it = SequenceDepth_Map.find(max_depth);
      if (it == SequenceDepth_Map.end() || it->second.empty()) {
        max_depth--;
        continue;
      }

      auto &curr_depth = it->second;
      while (!curr_depth.empty() && result.size() < N) {
        int id = curr_depth.back();
        result.push_back(std::pair<int, LFU_Block>(id, Store.at(id)));
        Store.erase(id);
        curr_depth.pop_back();
      }
      if (curr_depth.empty())
        max_depth--;
    }
    // for (auto &[id, block] : result) {
    //   std::cout << std::format("evicted id: {} frequency: {} last access: {}
    //   "
    //                            "sequence depth: {} prev: {} \n",
    //                            id, block.access_count, block.last_accessed,
    //                            block.sequence_depth, block.parent_id);
    // }
    return result;
  }

  void insert_or_update(int id, int ts, std::size_t sequence_depth,
                        int parent_id = -1) {
    auto it = Store.find(id);
    if (it != Store.end()) {
      hit++;
      // update block
      auto *block = &it->second;
      block->access_count++;
      block->last_accessed = ts;
    } else {
      miss++;
      // evict if full
      if (Store.size() == cache_size)
        evict(1);

      // max_depth is used to get current leaf node with biggest sequence depth
      // to be used for O(1) eviction
      if (max_depth < static_cast<int>(sequence_depth))
        max_depth = sequence_depth;

      // insert block
      if (!SequenceDepth_Map.contains(sequence_depth))
        SequenceDepth_Map.insert(std::pair<int, std::list<std::size_t>>(
            sequence_depth, std::list<std::size_t>()));
      auto &depth = SequenceDepth_Map.at(sequence_depth);
      auto it = depth.insert(depth.begin(), id);
      Store.insert(std::pair<int, LFU_Block>(
          id, {ts, ts, 1, sequence_depth, parent_id, it}));
    }
  }
};