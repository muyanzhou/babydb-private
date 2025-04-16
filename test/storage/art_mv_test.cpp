#include "gtest/gtest.h"
#include "storage/art.hpp"
#include "storage/table.hpp"      
#include "storage/index.hpp"     
#include "common/typedefs.hpp"    
#include <algorithm>
#include <random>
#include <vector>
#include <unordered_map>
#include <functional>
#include <limits>

using babydb::Table;
using babydb::Schema;
using babydb::Tuple;
using babydb::TupleMeta;
using babydb::data_t;
using babydb::idx_t;
const idx_t INVALID_ID = std::numeric_limits<idx_t>::max();

void BuildSortedTable(Table &table, idx_t count) {
    auto write_guard = table.GetWriteTableGuard();
    for (idx_t i = 0; i < count; i++) {
        write_guard.Rows().push_back({Tuple{static_cast<data_t>(i), static_cast<data_t>(i)}, TupleMeta()});
    }
    write_guard.Drop();
}

void BuildRandomTable(Table &table, idx_t count, int seed = 42) {
    std::vector<data_t> keys;
    keys.reserve(count);
    for (idx_t i = 1; i <= count; i++) {
        keys.push_back(i);
    }
    std::mt19937 rng(seed);
    std::shuffle(keys.begin(), keys.end(), rng);
    
    auto write_guard = table.GetWriteTableGuard();
    for (idx_t i = 0; i < count; i++) {
        write_guard.Rows().push_back({Tuple{static_cast<data_t>(keys[i]), static_cast<data_t>(i)}, TupleMeta()});
    }
    write_guard.Drop();
}

void BuildSparseTable(Table &table, idx_t count, int gap = 1000, int seed = 42) {
    std::vector<data_t> keys;
    keys.reserve(count);
    for (idx_t i = 1; i <= count; i++) {
        keys.push_back(static_cast<data_t>(i) * gap);
    }
    std::mt19937 rng(seed);
    std::shuffle(keys.begin(), keys.end(), rng);
    
    auto write_guard = table.GetWriteTableGuard();
    for (idx_t i = 0; i < count; i++) {
        write_guard.Rows().push_back({Tuple{keys[i], static_cast<data_t>(i)}, TupleMeta()});
    }
    write_guard.Drop();
}


std::unordered_map<data_t, idx_t> BuildKeyMapping(Table &table) {
    std::unordered_map<data_t, idx_t> mapping;
    auto read_guard = table.GetReadTableGuard();
    for (idx_t i = 0; i < read_guard.Rows().size(); i++) {
        data_t key = read_guard.Rows()[i].tuple_.KeyFromTuple(table.schema_.GetKeyAttr("c0"));
        mapping[key] = i; // row id is i
    }
    return mapping;
}

void VerifyRangeResult(const std::vector<idx_t>& result, const std::vector<idx_t>& expected) {
    std::vector<idx_t> sortedResult = result;
    std::vector<idx_t> sortedExpected = expected;
    std::sort(sortedResult.begin(), sortedResult.end());
    std::sort(sortedExpected.begin(), sortedExpected.end());
    EXPECT_EQ(sortedResult, sortedExpected);
}


namespace babydb {


TEST(Project1ArtIndexMVCC, SortedKeys_RangeQuery) {
    Schema schema{"c0", "c1"};
    Table table("sorted_range_query", schema);
    BuildSortedTable(table, 100000);
    ArtIndex index("art_sorted", table, "c0");
    auto mapping = BuildKeyMapping(table);
    std::vector<idx_t> result1;
    index.ScanRange({20000, 30000, true, true}, result1);
    std::vector<idx_t> expected1;
    for (idx_t i = 20000; i <= 30000; i++) {
        expected1.push_back(mapping[i]);
    }
    VerifyRangeResult(result1, expected1);

    std::vector<idx_t> result2;
    index.ScanRange({50000, 60000, false, false}, result2);
    std::vector<idx_t> expected2;
    for (idx_t i = 50000 + 1; i < 60000; i++) {
        expected2.push_back(mapping[i]);
    }
    VerifyRangeResult(result2, expected2);
}

TEST(Project1ArtIndexMVCC, SortedKeys_RangeQuery_MultipleRanges) {
    Schema schema{"c0", "c1"};
    Table table("sorted_multi_range", schema);
    BuildSortedTable(table, 100000);
    ArtIndex index("art_sorted_multi_range", table, "c0");
    auto mapping = BuildKeyMapping(table);

    for (idx_t round = 0; round < 10000; round++) {
        std::vector<idx_t> result;
        index.ScanRange({round * 10, round * 10 + 5, true, true}, result);
        std::vector<idx_t> expected;
        for (idx_t i = round * 10; i <= round * 10 + 5; i++) {
            expected.push_back(mapping[i]);
        }
        VerifyRangeResult(result, expected);
    }
}

TEST(Project1ArtIndexMVCC, RandomKeys_OnlyPointQuery) {
    Schema schema{"c0", "c1"};
    Table table("random_only_point", schema);
    BuildRandomTable(table, 100000, 123);
    ArtIndex index("art_random", table, "c0");
    auto mapping = BuildKeyMapping(table);
    std::vector<data_t> testKeys = {10, 50000, 100000};
    for (data_t k : testKeys) {
        EXPECT_EQ(index.LookupKey(k), mapping[k]);
    }
}

TEST(Project1ArtIndexMVCC, RandomKeys_RangeQuery) {
    Schema schema{"c0", "c1"};
    Table table("random_range_query", schema);
    BuildRandomTable(table, 100000, 456);
    ArtIndex index("art_random", table, "c0");
    auto mapping = BuildKeyMapping(table);
    std::vector<data_t> allKeys;
    {
        auto read_guard = table.GetReadTableGuard();
        for (const auto &row : read_guard.Rows())
            allKeys.push_back(row.tuple_.KeyFromTuple(table.schema_.GetKeyAttr("c0")));
    }
    std::sort(allKeys.begin(), allKeys.end());
    data_t low = allKeys.front() + 1000;
    data_t high = allKeys.front() + 5000;
    std::vector<idx_t> expected;
    for (data_t key : allKeys) {
        if (key >= low && key <= high)
            expected.push_back(mapping[key]);
    }
    std::vector<idx_t> result;
    index.ScanRange({low, high, true, true}, result);
    VerifyRangeResult(result, expected);
}

TEST(Project1ArtIndexMVCC, SparseKeys_OnlyPointQuery) {
    Schema schema{"c0", "c1"};
    Table table("sparse_only_point", schema);
    BuildSparseTable(table, 100000, 10000);
    ArtIndex index("art_sparse", table, "c0");
    auto mapping = BuildKeyMapping(table);
    for (idx_t i = 1; i <= 100000; i += 10000) {
        data_t key = static_cast<data_t>(i) * 10000;
        EXPECT_EQ(index.LookupKey(key), mapping[key]);
    }
}

TEST(Project1ArtIndexMVCC, SparseKeys_RangeQuery) {
    Schema schema{"c0", "c1"};
    Table table("sparse_range_query", schema);
    BuildSparseTable(table, 100000, 10000, 890);
    ArtIndex index("art_sparse", table, "c0");
    auto mapping = BuildKeyMapping(table);
    std::vector<idx_t> result;
    index.ScanRange({10000000, 50000000, true, true}, result);
    std::vector<idx_t> expected;
    {
        auto read_guard = table.GetReadTableGuard();
        for (const auto &row : read_guard.Rows()) {
            data_t k = row.tuple_.KeyFromTuple(table.schema_.GetKeyAttr("c0"));
            if (k >= 10000000 && k <= 50000000)
                expected.push_back(mapping[k]);
        }
    }
    VerifyRangeResult(result, expected);
}

TEST(Project1ArtIndexMVCC, DenseKeys_WithUpdates_PointQuery) {
    Schema schema{"c0", "c1"};
    Table table("dense_updates_point", schema);
    BuildSortedTable(table, 100000);
    ArtIndex index("art_dense", table, "c0");

    index.InsertEntry(50000, 500000, 50);
    index.InsertEntry(50000, 500001, 100);
    index.InsertEntry(50000, 500002, 150);
    EXPECT_EQ(index.LookupKey(50000, 75), 500000);
    EXPECT_EQ(index.LookupKey(50000, 100), 500001);
    EXPECT_EQ(index.LookupKey(50000, 200), 500002);
    EXPECT_EQ(index.LookupKey(50000, 40), 50000);
}

TEST(Project1ArtIndexMVCC, MixedReadWrite_HighQueryRatio) {
    Schema schema{"c0", "c1"};
    Table table("mixed_high_query", schema);
    const idx_t count = 10000;
    BuildRandomTable(table, count, 789);
    ArtIndex index("art_mixed_high", table, "c0");
    auto mapping = BuildKeyMapping(table);
    std::vector<data_t> allKeys;
    {
        auto read_guard = table.GetReadTableGuard();
        for (const auto &row : read_guard.Rows())
            allKeys.push_back(row.tuple_.KeyFromTuple(table.schema_.GetKeyAttr("c0")));
    }
    std::sort(allKeys.begin(), allKeys.end());
    std::mt19937 rng(789);
    std::uniform_int_distribution<> dist(0, allKeys.size() - 1);
    auto cur_rowid = count;
    for (idx_t i = 0; i < 100000; i++) {
        idx_t idx = dist(rng);
        data_t key = allKeys[idx];
        EXPECT_EQ(index.LookupKey(key, cur_rowid), mapping[key]);
        index.InsertEntry(key, cur_rowid, cur_rowid);
        mapping[key] = cur_rowid;
        cur_rowid++;
    }
}

TEST(Project1ArtIndexMVCC, LongVersionChain_SequentialTs) {
    Schema schema{"c0", "c1"};
    Table table("long_version_chain_all", schema);
    const idx_t count = 5;

    BuildSortedTable(table, count);
    ArtIndex index("art_long_chain_all", table, "c0");
    
    const idx_t numUpdates = 100000;

    for (idx_t key = 1; key < count; key++) {
        for (idx_t i = numUpdates; i-- != 0; ) {
            index.InsertEntry(key, key * 1000000 + i, 100 + i);
        }
    }

    idx_t queryTs = 100 + numUpdates / 2;

    for (idx_t key = 1; key < count; key++) {
        idx_t expected = key * 1000000 + (queryTs - 100);
        EXPECT_EQ(index.LookupKey(key, queryTs), expected);
    }
}

TEST(Project1ArtIndexMVCC, LongVersionChain_RandomTs) {
    Schema schema{"c0", "c1"};
    Table table("long_version_chain_all", schema);
    const idx_t count = 5;

    BuildSortedTable(table, count);
    ArtIndex index("art_long_chain_all", table, "c0");
    
    const idx_t numUpdates = 100000;

    std::vector<idx_t> updates;
    for (idx_t i = 0; i < numUpdates; i++) {
        updates.push_back(i);
    }
    const int seed = 33244;
    std::mt19937 rnd(seed);
    std::shuffle(updates.begin(), updates.end(), rnd);
    for (auto i : updates) {
        for (idx_t key = 1; key < count; key++) {
            index.InsertEntry(key, key * 1000000 + i, 100 + i);
        }
    }

    std::shuffle(updates.begin(), updates.end(), rnd);

    for (auto queryTs : updates) {
        for (idx_t key = 1; key < count; key++) {
            idx_t expected = key * 1000000 + queryTs;
            EXPECT_EQ(index.LookupKey(key, 100 + queryTs), expected);
        }
    }
}

TEST(Project1ArtIndexMVCC, LongVersionChain_SequentialTs_RangeQuery) {
    Schema schema{"c0", "c1"};
    Table table("long_version_chain_all", schema);
    const idx_t count = 100000;

    BuildSortedTable(table, count);
    ArtIndex index("art_long_chain_all", table, "c0");

    const idx_t numUpdates = 100000;

    for (idx_t key = 1; key < 10; key++) {
        for (idx_t i = numUpdates; i-- != 0; ) {
            index.InsertEntry(key * 10000, key * 1000000 + i, 100 + i);
        }
    }

    idx_t queryTs = 100 + numUpdates / 2;

    for (idx_t key = 1; key < 10; key++) {
        idx_t expected_special_key = key * 1000000 + (queryTs - 100);
        std::vector<idx_t> expected_key_list{expected_special_key};
        for (int delta = -2; delta <= 2; delta++) {
            if (delta != 0) {
                expected_key_list.push_back(key * 10000 + delta);
            }
        }
        std::vector<idx_t> result;
        index.ScanRange({key * 10000 - 2, key * 10000 + 2}, result, queryTs);
        VerifyRangeResult(result, expected_key_list);
    }

    const int seed = 42395;
    std::mt19937 rnd(seed);
    std::uniform_int_distribution<idx_t> start_dist(0, count - 10);
    std::uniform_int_distribution<idx_t> len_dist(1, 8);
    for (idx_t round = 0; round < numUpdates; round++) {
        idx_t start, end;
        do {
            start = start_dist(rnd);
            end = start + len_dist(rnd);
        } while (start / 10000 != end / 10000);
        std::vector<idx_t> result;
        std::vector<idx_t> expected;
        index.ScanRange({start, end, false, true}, result);
        for (idx_t i = start + 1; i <= end; i++) {
            expected.push_back(i);
        }
        VerifyRangeResult(result, expected);
    }
}

TEST(Project1ArtIndexMVCC, LongVersionChain_RandomTs_RangeQuery) {
    Schema schema{"c0", "c1"};
    Table table("long_version_chain_all", schema);
    const idx_t count = 100000;
    const idx_t special_count = 10;

    BuildSortedTable(table, count);
    ArtIndex index("art_long_chain_all", table, "c0");

    const idx_t numUpdates = 100000;

    std::vector<idx_t> updates;
    for (idx_t i = 0; i < numUpdates; i++) {
        updates.push_back(i);
    }
    const int seed = 45346;
    std::mt19937 rnd(seed);
    std::shuffle(updates.begin(), updates.end(), rnd);
    for (auto i : updates) {
        for (idx_t key = 1; key < special_count; key++) {
            index.InsertEntry(key * 10000, key * 1000000 + i, 100 + i);
        }
    }

    std::shuffle(updates.begin(), updates.end(), rnd);

    std::uniform_int_distribution<idx_t> key_dist(1, special_count - 1);
    for (auto queryTs : updates) {
        auto key = key_dist(rnd);
        idx_t expected_special = key * 1000000 + queryTs;
        std::vector<idx_t> expected_key_list{expected_special};
        std::vector<idx_t> result;
        for (int delta = -2; delta <= 2; delta++) {
            if (delta != 0) {
                expected_key_list.push_back(key * 10000 + delta);
            }
        }
        index.ScanRange({key * 10000 - 2, key * 10000 + 2}, result, 100 + queryTs);
        VerifyRangeResult(result, expected_key_list);
    }
}

} // namespace babydb
