#pragma once

#include "common/typedefs.hpp"
#include "storage/index.hpp"

#include <map>

namespace babydb {

class StlmapIndex : public RangeIndex {
public:
    explicit StlmapIndex(const std::string &name, Table &table, const std::string &key_name);

    ~StlmapIndex() override {};

    void InsertEntry(const data_t &key, idx_t row_id, idx_t start_ts = 0) override;

    void EraseEntry(const data_t &key) override;

    idx_t LookupKey(const data_t &key, idx_t query_ts = 0) override;

    void ScanRange(const RangeInfo &range, std::vector<idx_t> &row_ids, idx_t query_ts = 0) override;

private:
    std::map<data_t, idx_t> index_;
};

}
