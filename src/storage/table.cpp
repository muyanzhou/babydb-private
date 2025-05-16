#include "storage/table.hpp"
#include "concurrency/transaction.hpp"
#include <iostream>
namespace babydb {

Row::Row(const Transaction &txn, const Tuple &tuple, idx_t row_id, Table *table)
    : uncommitted_txn_id_(txn.txn_id_), rid(row_id), uncommitted_version_(tuple), version_list_(new VersionSkipList()), table_(table) {}
void Row::tryInsert(const Tuple &tuple, Transaction &txn) {
    std::unique_lock<std::shared_mutex> lock(latch_);
    if (uncommitted_txn_id_ == INVALID_ID || uncommitted_txn_id_ == txn.txn_id_) {
        uncommitted_txn_id_ = txn.txn_id_;
        uncommitted_version_ = tuple;
    }
    else {
        lock.unlock();
        txn.SetTainted();
        throw TaintedException("Row is tainted, please retry");
    }
}
void Row::insert(idx_t ts) {
    std::unique_lock<std::shared_mutex> lock(latch_);
    if (uncommitted_txn_id_ != INVALID_ID) {
        RegisterVersionNode();
        version_list_->insert(uncommitted_version_, ts);
        uncommitted_txn_id_ = INVALID_ID;
    }
    else {
        lock.unlock();
        throw std::logic_error("No uncommitted version to insert");
    }
}
void Row::abort() {
    std::unique_lock<std::shared_mutex> lock(latch_);
    uncommitted_txn_id_ = INVALID_ID;
    uncommitted_version_ = Tuple();
}
Tuple Row::query(Transaction &txn) const {
    {
        std::shared_lock<std::shared_mutex> lock(latch_);
        if (uncommitted_txn_id_ == txn.txn_id_) {
            return uncommitted_version_;
        }
    }
    txn.AddReadRow(rid, table_);
    auto version = version_list_->query(txn.read_ts_);
    return version;
}
bool Row::check(const Transaction &txn) const {
    return version_list_->check(txn.read_ts_);
}

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