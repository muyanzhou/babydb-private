#pragma once

#include "common/typedefs.hpp"
#include "transaction.hpp"

#include <memory>
#include <unordered_map>

namespace babydb {

class TransactionManager {
public:
    TransactionManager(IsolationLevel isolation_level = IsolationLevel::SNAPSHOT) : isolation_level_(isolation_level) {}
    //! Create a new transaction.
    std::shared_ptr<Transaction> CreateTxn(std::shared_lock<std::shared_mutex> &&db_lock);
    //! Commit a transaction, return false if aborted.
    bool Commit(Transaction &txn);
    //! Abort a transaction.
    void Abort(Transaction &txn);

private:
    //! Garbage collection, delete the versions that are invisible even to the oldest running transaction.
    void GC(WriteTableGuard &write_guard, idx_t row_id);
    bool VerifyTxn(Transaction &txn);

private:
    idx_t next_txn_id_{TXN_START_ID};

    idx_t last_commit_ts_{0};
    //! The oldest read timestamp, used for garbage collection.
    idx_t oldest_read_ts_{0};

    std::unordered_map<idx_t, std::shared_ptr<Transaction>> txn_map_;

    std::shared_mutex txn_map_latch_;

    std::mutex commit_latch_;

    std::unordered_map<idx_t, idx_t> running_txns_;

    std::mutex running_txns_latch_;

    const IsolationLevel isolation_level_;
};

}