#include "storage/stlmap_index.hpp"

namespace babydb {

StlmapIndex::StlmapIndex(const std::string &name, Table &table, const std::string &key_name)
    : RangeIndex(name, table, std::move(key_name)) {
    auto read_guard = table.GetReadTableGuard();
    auto &rows = read_guard.Rows();
    auto key_attr = table.schema_.GetKeyAttr(key_name);
    for (idx_t row_id = 0; row_id < rows.size(); row_id++) {
        auto &row = rows[row_id];
        if (!row.tuple_meta_.is_deleted_) {
            InsertEntry(row.tuple_.KeyFromTuple(key_attr), row_id);
        }
    }
}

void StlmapIndex::InsertEntry(const data_t &key, idx_t row_id, idx_t start_ts) {
    if (index_.find(key) != index_.end()) {
        throw std::logic_error("duplicated key");
    }
    index_[key] = row_id;
};

void StlmapIndex::EraseEntry(const data_t &key) {
    index_.erase(key);
};

idx_t StlmapIndex::LookupKey(const data_t &key, idx_t query_ts) {
    auto ite = index_.find(key);
    if (ite == index_.end()) {
        return INVALID_ID;
    }
    return ite->second;
}

void StlmapIndex::ScanRange(const RangeInfo &range, std::vector<idx_t> &row_ids, idx_t query_ts) {
    row_ids.clear();
    std::map<data_t, idx_t>::iterator start_ite;
    std::map<data_t, idx_t>::iterator end_ite;
    if (range.contain_start) {
        start_ite = index_.lower_bound(range.start);
    } else {
        start_ite = index_.upper_bound(range.start);
    }
    if (range.contain_end) {
        end_ite = index_.upper_bound(range.end);
    } else {
        end_ite = index_.lower_bound(range.end);
    }
    for (auto ite = start_ite; ite != end_ite; ite++) {
        row_ids.push_back(ite->second);
    }
}

}
