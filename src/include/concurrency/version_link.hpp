#pragma once
#include <random>

#include "common/typedefs.hpp"

namespace babydb {

void RegisterVersionNode();

void UnregisterVersionNode();

struct SkipListNode {
    idx_t row_id, timestamp, level;
    SkipListNode *next, *down;
    SkipListNode(idx_t ri, idx_t ts, idx_t l, SkipListNode* n, SkipListNode* d) : row_id(ri), timestamp(ts), level(l), next(n), down(d) {}
};

class VersionSkipList {
private:
    static const uint32_t SKIPLIST_LEVEL = 20;
    uint32_t level;
    std::mt19937 rd;
    SkipListNode* head[SKIPLIST_LEVEL];
    SkipListNode* tail[SKIPLIST_LEVEL];
    uint32_t randomLevel();
public:
    VersionSkipList();
    ~VersionSkipList();
    void insert(idx_t row_id, idx_t timestamp);
    idx_t query(idx_t ts);
};

}