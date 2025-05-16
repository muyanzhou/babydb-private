#include "concurrency/transaction_manager.hpp"
#include "concurrency/version_link.hpp"

namespace babydb {

std::shared_ptr<Transaction> TransactionManager::CreateTxn(std::shared_lock<std::shared_mutex> &&db_lock) {
    std::unique_lock lock(txn_map_latch_);
    auto result = std::make_shared<Transaction>(next_txn_id_, last_commit_ts_, last_commit_ts_, std::move(db_lock));
    txn_map_.insert(std::make_pair(next_txn_id_, result));
    if (running_txns_.find(result->gc_ts_) == running_txns_.end()) {
        running_txns_.insert(std::make_pair(result->gc_ts_, 0));
    }
    running_txns_[result->gc_ts_]++;
    next_txn_id_++;
    return result;
}

void TransactionManager::GC(WriteTableGuard &write_guard, idx_t row_id) {
    idx_t temp = oldest_read_ts_;
    //! Find the oldest running transaction.
    for (; temp < last_commit_ts_; temp++) {
        auto it = running_txns_.find(temp);
        if (it == running_txns_.end()) continue;
        if (it->second == 0) running_txns_.erase(it);
        else break;
    }
    oldest_read_ts_ = temp;
    //! Delete the versions that are invisible even to the oldest running transaction.
    write_guard.Rows()[row_id].version_list_->erase(oldest_read_ts_);
}

bool TransactionManager::VerifyTxn([[maybe_unused]] Transaction &txn) {
    if (txn.ReadOnly()) return true;
    
    switch (isolation_level_) {
        case IsolationLevel::SNAPSHOT:
            for (auto &[row, table] : txn.modified_rows_) {
                auto read_guard = table->GetReadTableGuard();
                if (!read_guard.Rows()[row].check(txn)) {
                    read_guard.Drop();
                    return false;
                }
                read_guard.Drop();
            }
            break;
        case IsolationLevel::SERIALIZABLE:
            //! Forward check.
            for (auto &[row, table] : txn.read_rows_) {
                auto read_guard = table->GetReadTableGuard();
                if (!read_guard.Rows()[row].check(txn)) {
                    read_guard.Drop();
                    return false;
                }
                read_guard.Drop();
            }
            //! Backward check.
            for (auto &[row, table] : txn.modified_rows_) {
                auto read_guard = table->GetReadTableGuard();
                if (!read_guard.Rows()[row].check(txn)) {
                    read_guard.Drop();
                    return false;
                }
                read_guard.Drop();
            }
            break;
        default:
            throw std::logic_error("No such isolation level.");
            break;
    }
    return true;
}

bool TransactionManager::Commit(Transaction &txn) {
    if (txn.state_ != RUNNING) {
        throw std::logic_error("Try to commit a not running transaction."); 
    }
    std::unique_lock commit_lock(commit_latch_);
    if (!VerifyTxn(txn)) {
        commit_lock.unlock();
        Abort(txn);
        return false;
    }
    std::unique_lock map_lock(txn_map_latch_);
    txn.commit_ts_ = ++last_commit_ts_;
    txn.state_ = COMMITED;
    //! Commit the transaction.
    std::map<std::pair<idx_t, Table *>, bool> committed;
    for (auto &[row, table] : txn.modified_rows_) {
        if (committed.find(std::make_pair(row, table)) != committed.end()) continue;
        committed[{row, table}] = true;
        auto write_guard = table->GetWriteTableGuard();
        assert(write_guard.Rows()[row].uncommitted_txn_id_ == txn.txn_id_);
        GC(write_guard, row);
        write_guard.Rows()[row].insert(txn.commit_ts_);
        write_guard.Drop();
    }
    txn.Done();
    txn_map_.erase(txn.txn_id_);
    running_txns_[txn.gc_ts_]--;
    return true;
}

void TransactionManager::Abort(Transaction &txn) {
    if (txn.state_ != RUNNING && txn.state_ != TAINTED) {
        throw std::logic_error("Try to abort a not running or tainted transaction."); 
    }
    //! Abort the transaction.
    for (auto &[row, table] : txn.modified_rows_) {
        auto write_guard = table->GetWriteTableGuard();
        write_guard.Rows()[row].abort();
        write_guard.Drop();
    }
    std::unique_lock map_lock(txn_map_latch_);
    txn.state_ = ABORTED;
    txn.Done();
    txn_map_.erase(txn.txn_id_);
    running_txns_[txn.gc_ts_]--;
}

}