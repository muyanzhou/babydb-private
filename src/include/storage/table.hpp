#pragma once

#include "common/typedefs.hpp"
#include "common/macro.hpp"
#include "concurrency/version_link.hpp"

#include <mutex>
#include <set>
#include <string>
#include <shared_mutex>
#include <vector>
#include <map>

namespace babydb {

struct TupleMeta {

};

class Table;

struct Row {
    idx_t uncommitted_txn_id_{INVALID_ID}, rid{0};
    Tuple uncommitted_version_;
    VersionSkipList *version_list_{nullptr};
    Table *table_{nullptr};
    mutable std::shared_mutex latch_;
    mutable std::mutex map_latch_;
    Row(const Row&) = delete;
    Row& operator=(const Row&) = delete;

    Row(Row&& other) noexcept 
        : uncommitted_txn_id_(other.uncommitted_txn_id_),
        rid(other.rid),
        uncommitted_version_(std::move(other.uncommitted_version_)),
        version_list_(other.version_list_),
        table_(other.table_) {
        other.version_list_ = nullptr;
        other.table_ = nullptr;
    }

    Row& operator=(Row&& other) noexcept {
        if (this != &other) {
            delete version_list_;
            uncommitted_txn_id_ = other.uncommitted_txn_id_;
            rid = other.rid;
            uncommitted_version_ = std::move(other.uncommitted_version_);
            version_list_ = other.version_list_;
            table_ = other.table_;

            other.version_list_ = nullptr;
            other.table_ = nullptr;
        }
        return *this;
    }
    Row(const Transaction &txn, const Tuple &tuple, idx_t row_id, Table *table);
    ~Row() { delete version_list_; }
    void tryInsert(const Tuple &tuple, Transaction &txn);//! Try to insert a new version. This version is temporarily not inserted into the skip list.
    void insert(idx_t ts);//! Insert the version into the skip list.
    void abort();
    Tuple query(Transaction &txn) const;
    bool check(const Transaction &txn) const;//! Check if the version txn read is the latest version.
};

class ReadTableGuard;
class WriteTableGuard;

/** 
 * When access the table's rows, you should hold the latch of the table.
 * We design the table guard. When you hold the table guard, you can safely use the rows.
 * Since it's only a latch, you need to make sure,
 * 1. At any time, you can hold at most 1 table guard.
 * 2. When you get the table guard, drop it after finite instructions
 *    (so you should not require another latch during holding the guard).
 * 3. When you hold the table guard, you should keep using the table,
 *    otherwise you should drop it and require it later.
 */
class Table {
public:
    const std::string name_;

    const Schema schema_;

public:
    explicit Table(const std::string &name, const Schema &schema)
        : name_(name), schema_(schema) {}

    DISALLOW_COPY_AND_MOVE(Table);
    //! Get the read permission to the table.
    ReadTableGuard GetReadTableGuard();
    //! Get the read and write permission to the table.
    WriteTableGuard GetWriteTableGuard();

    const std::string GetIndex() const {
        return index_name_;
    }

private:
    std::vector<Row> rows_;
    //! Empty string means no index. To simplify, a table can have at most 1 index.
    std::string index_name_; 

    std::shared_mutex latch_;

friend class Catalog;
};

class ReadTableGuard {
public:
    explicit ReadTableGuard(const std::vector<Row> &rows, std::shared_mutex &latch)
        : rows_(&rows), latch_(latch) {
        latch_.lock_shared();
    }

    ~ReadTableGuard() { Drop(); }

    DISALLOW_COPY(ReadTableGuard);

    void Drop();

    const std::vector<Row>& Rows() { return *rows_; }

private:
    const std::vector<Row> *rows_;

    std::shared_mutex& latch_;

    bool drop_tag_{false};
};

class WriteTableGuard {
public:
    explicit WriteTableGuard(std::vector<Row> &rows, std::shared_mutex &latch)
        : rows_(&rows), latch_(latch) {
        latch_.lock();
    }

    ~WriteTableGuard() { Drop(); }

    DISALLOW_COPY(WriteTableGuard);

    void Drop();

    std::vector<Row>& Rows() { return *rows_; }

private:
    std::vector<Row> *rows_;

    std::shared_mutex &latch_;

    bool drop_tag_{false};
};

}