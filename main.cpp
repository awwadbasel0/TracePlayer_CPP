#include "./KV_Store.hpp"
#include "./trace_replayer.hpp"
#include <cstddef>
#include <cstdio>
#include <limits>
#include <print>
#include <vector>

std::size_t CACHE_SIZE = 16000; // in blocks

int main(int argc, char **argv) {
  TracePlayer TP("/home/shado/Projects/Thesis_Research/opal-sim/traces/"
                 "conversation_trace_mooncake.jsonl");
  Trace *T = TP.step();
  // KV_Store store;
  LRU_Store lruStore(CACHE_SIZE);
  LFU_Store lfuStore(CACHE_SIZE);
  LengthAware_Store lAStore(CACHE_SIZE);
  while (T) {

    int prev_id = -1;
    std::size_t curr_seq_length = 0;
    auto hashids = T->hash_ids;
    for (auto id : hashids) {

      lruStore.insert_or_update(id, T->timestamp, curr_seq_length, prev_id);
      lfuStore.insert_or_update(id, T->timestamp, curr_seq_length, prev_id);
      lAStore.insert_or_update(id, T->timestamp, curr_seq_length, prev_id);

      curr_seq_length++;
      prev_id = id;
    }
    T = TP.step();
  }
  std::print("LRU : [ hit:{} miss:{} ]\nLFU : [ hit:{} miss:{} ]\nLength_Aware "
             ": [ hit:{} miss:{} ]\n",
             lruStore.hit, lruStore.miss, lfuStore.hit, lfuStore.miss,
             lAStore.hit, lAStore.miss);
  return 0;
}