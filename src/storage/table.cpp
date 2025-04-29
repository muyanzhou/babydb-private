#include "storage/table.hpp"

namespace babydb {

void ReadTableGuard::Drop() {
    if (!drop_tag_) {
        rows_ = nullptr;
        latch_.unlock_shared();
        drop_tag_ = true;
    }
}

void WriteTableGuard::Drop() {
    if (!drop_tag_) {
        rows_ = nullptr;
        latch_.unlock();
        drop_tag_ = true;
    }
}

ReadTableGuard Table::GetReadTableGuard() {
    return ReadTableGuard(rows_, latch_);
}

WriteTableGuard Table::GetWriteTableGuard() {
    return WriteTableGuard(rows_, latch_);
}

}