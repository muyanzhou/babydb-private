#include "concurrency/version_link.hpp"

#include <atomic>
#include <mutex>
#include <random>

namespace babydb {

VersionSkipList::VersionSkipList() : level(0), write_ts_(0), rd(7) {
    for (uint32_t i = 0; i < SKIPLIST_LEVEL; i++) {
        tail[i] = new Version(Tuple(), 1ULL << 32, i, nullptr, i ? tail[i - 1] : nullptr);
        head[i] = new Version(Tuple(), 0, i, tail[i], i ? head[i - 1] : nullptr);
    }
}
VersionSkipList::~VersionSkipList() {
    for (uint32_t i = 0; i < SKIPLIST_LEVEL; i++) {
        Version* cur = head[i];
        while (cur != nullptr) {
            Version* tmp = cur->next;
            delete cur;
            cur = tmp;
        }
    }
}
uint32_t VersionSkipList::randomLevel() {
    uint32_t lvl = 0;
    while (lvl < SKIPLIST_LEVEL - 1 && (rd() & 3) == 0) lvl++;
    return lvl;
}
Version* VersionSkipList::find(idx_t ts) const {
    Version* current = head[level];
    for (int i = level; i >= 0; i--) {
        while (current->next != tail[i] && current->next->timestamp <= ts)
            current = current->next;
        if (current->timestamp == ts) break;
        if (i > 0) current = current->down;
    }
    return current;
}

void VersionSkipList::insert(const Tuple&tp, idx_t timestamp) {
    std::unique_lock lock(latch_);
    write_ts_ = std::max(write_ts_, timestamp);

    Version* update[SKIPLIST_LEVEL];
    Version* current = head[level];
    
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
    
    Version* bottom_node = nullptr;
    for (uint32_t i = 0; i <= new_level; i++) {
        Version* new_node = new Version(tp, timestamp, i, update[i]->next, nullptr);
        if (i > 0) new_node->down = bottom_node;
        update[i]->next = new_node;
        bottom_node = new_node;
    }
}
void VersionSkipList::erase(idx_t ts) {
    //! Find the earliest visible version, and delete all versions that are older than it.
    std::shared_lock lock(latch_);
    auto cur = find(ts);
    while (cur->down != nullptr) cur = cur->down;
    if (cur == head[0] || head[0]->next == cur) {
        lock.unlock();
        return;
    }
    idx_t max_ts = cur->timestamp;
    lock.unlock();

    std::unique_lock lock_(latch_);
    for (int i = level; i >= 0; i--) {
        while (head[i]->next != tail[i] && head[i]->next->timestamp < max_ts) {
            Version* cur = head[i]->next;
            head[i]->next = cur->next;
            if (i == 0) UnregisterVersionNode();
            delete cur;
        }
        if (static_cast<uint32_t>(i) == level && head[i]->next == tail[i] && level > 0) level--;
    }
}
Tuple VersionSkipList::query(idx_t ts) {
    std::shared_lock lock(latch_);
    return find(ts)->tuple;
}
bool VersionSkipList::check(idx_t ts) {
    std::shared_lock lock(latch_);
    return ts >= write_ts_;
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