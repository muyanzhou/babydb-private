#include "concurrency/version_link.hpp"

#include <atomic>
#include <random>

namespace babydb {

VersionSkipList::VersionSkipList() : level(0), rd(std::random_device{}()) {
    for (uint32_t i = 0; i < SKIPLIST_LEVEL; i++) {
        tail[i] = new SkipListNode(0, 1ULL << 32, i, nullptr, i ? tail[i - 1] : nullptr);
        head[i] = new SkipListNode(0, 0, i, tail[i], i ? head[i - 1] : nullptr);
    }
}
VersionSkipList::~VersionSkipList() {
    for (uint32_t i = 0; i < SKIPLIST_LEVEL; i++) {
        SkipListNode* cur = head[i];
        while (cur != nullptr) {
            SkipListNode* tmp = cur->next;
            delete cur;
            cur = tmp;
        }
    }
}
uint32_t VersionSkipList::randomLevel() {
    uint32_t lvl = 0;
    while (lvl < SKIPLIST_LEVEL - 1 && (rd() & 1)) lvl++;
    return lvl;
}

void VersionSkipList::insert(idx_t row_id, idx_t timestamp) {
    SkipListNode* update[SKIPLIST_LEVEL];
    SkipListNode* current = head[level];
    
    for (int i = level; i >= 0; i--) {
        while (current->next != tail[i] && current->next->timestamp < timestamp)
            current = current->next;
        update[i] = current;
        if (i > 0) current = current->down;
    }
    uint32_t new_level = randomLevel();
    if (new_level > level) {
        for (uint32_t i = level + 1; i <= new_level; i++)
            update[i] = head[i];
        level = new_level;
    }
    
    SkipListNode* bottom_node = nullptr;
    for (uint32_t i = 0; i <= new_level; i++) {
        SkipListNode* new_node = new SkipListNode(row_id, timestamp, i, update[i]->next, nullptr);
        if (i > 0) new_node->down = bottom_node;
        update[i]->next = new_node;
        bottom_node = new_node;
    }
}

idx_t VersionSkipList::query(idx_t ts) {
    SkipListNode* current = head[level];
    for (int i = level; i >= 0; i--) {
        while (current->next != tail[i] && current->next->timestamp <= ts)
            current = current->next;
        if (current->timestamp == ts) return current->row_id;            
        if (i > 0) current = current->down;
    }
    return current->row_id;
}

// START: Do not modify this part.

std::atomic<idx_t> current_nodes{0};
std::atomic<idx_t> max_nodes{0};

void RegisterVersionNode() {
    auto update = current_nodes.fetch_add(1, std::memory_order_relaxed) + 1;
    auto expected = max_nodes.load(std::memory_order_relaxed);
    while (update > expected && !max_nodes.compare_exchange_weak(expected, update, std::memory_order_relaxed));
}

void UnregisterVersionNode() {
    current_nodes.fetch_sub(1, std::memory_order_relaxed);
}

// END: Do not modify this part.

}