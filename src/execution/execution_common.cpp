#include "execution/execution_common.hpp"

#include "storage/index.hpp"
#include "storage/table.hpp"

namespace babydb {


void DeleteRow(WriteTableGuard &write_guard, idx_t row_id) {
    write_guard.Rows()[row_id].tuple_meta_.is_deleted_ = true;
}

void InsertRowWithIndex(WriteTableGuard &write_guard, Tuple &&tuple, Index *index, const data_t &key) {
    auto row_id = index->LookupKey(key);
    // Key is existed, but deleted
    if (row_id != INVALID_ID) {
        auto &[data, meta] = write_guard.Rows()[row_id];
        if (!meta.is_deleted_) {
            throw std::logic_error("InsertOperator: Duplicated key");
        }
        meta.is_deleted_ = false;
        data = std::move(tuple);
        return;
    }

    // Just create a new row
    row_id = write_guard.Rows().size();
    index->InsertEntry(key, row_id);
    InsertRowWoIndex(write_guard, std::move(tuple));
}

void InsertRowWoIndex(WriteTableGuard &write_guard, Tuple &&tuple) {
    TupleMeta tuple_meta;
    write_guard.Rows().push_back({std::move(tuple), tuple_meta});
}

}