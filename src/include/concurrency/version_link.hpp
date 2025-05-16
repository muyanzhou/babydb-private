#pragma once
#include <random>

#include "common/typedefs.hpp"
#include <shared_mutex>

namespace babydb {

void RegisterVersionNode();

void UnregisterVersionNode();

struct Version {
    idx_t timestamp, level;
    Tuple tuple;
    Version *next, *down;
    Version(const Tuple&tp, idx_t ts, idx_t l, Version* n, Version* d) : timestamp(ts), level(l), tuple(tp), next(n), down(d) {}
};

class VersionSkipList {
private:
    static const uint32_t SKIPLIST_LEVEL = 8;
    uint32_t level;
    idx_t write_ts_;//! The timestamp of the last write operation.
    std::mt19937 rd;
    mutable std::shared_mutex latch_;
    Version* head[SKIPLIST_LEVEL];
    Version* tail[SKIPLIST_LEVEL];
    uint32_t randomLevel();
    Version* find(idx_t ts) const;//! Find the version with the largest timestamp less than or equal to ts without locking.
public:
    VersionSkipList();
    ~VersionSkipList();
    void insert(const Tuple&tp, idx_t timestamp);
    void erase(idx_t ts);
    Tuple query(idx_t ts);
    bool check(idx_t ts);
};

}