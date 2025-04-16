#include "gtest/gtest.h"

#include "babydb.hpp"
#include "execution/delete_operator.hpp"
#include "execution/insert_operator.hpp"
#include "execution/filter_operator.hpp"
#include "execution/seq_scan_operator.hpp"
#include "execution/projection_operator.hpp"
#include "execution/range_index_scan_operator.hpp"
#include "execution/update_operator.hpp"
#include "execution/value_operator.hpp"
#include "storage/catalog.hpp"
#include "storage/index.hpp"
#include "storage/table.hpp"

#include <algorithm>

namespace babydb {

static ConfigGroup TestConfig() {
    ConfigGroup config;
    config.CHUNK_SUGGEST_SIZE = 2;
    return config;
}

static std::vector<Tuple> RunOperator(Operator &test_operator, bool sort_output = true) {
    test_operator.Check();
    test_operator.Init();
    std::vector<Tuple> results;
    Chunk chunk;
    auto operator_state = OperatorState::HAVE_MORE_OUTPUT;
    while (operator_state != EXHAUSETED) {
        operator_state = test_operator.Next(chunk);
        for (auto &row : chunk) {
            results.push_back(row.first);
        }
    }
    if (sort_output) {
        std::sort(results.begin(), results.end());
    }
    return results;
}

TEST(TableOperatorTest, InsertAndScanBasicTest) {
    BabyDB test_db(TestConfig());
    Schema schema{"c0", "c1"};
    test_db.CreateTable("table0", schema);
    test_db.CreateIndex("index0_0", "table0", "c0", IndexType::Stlmap);
    auto txn = test_db.CreateTxn();
    auto exec_ctx = test_db.GetExecutionContext(txn);

    std::vector<Tuple> insert_tuples;
    insert_tuples.push_back({0, 1});
    insert_tuples.push_back({2, 3});
    insert_tuples.push_back({4, 5});

    std::vector<Tuple> insert_tuples_copy = insert_tuples;

    auto value_operator = std::make_shared<ValueOperator>(exec_ctx, Schema{"c0", "c1"}, std::move(insert_tuples_copy));
    auto insert_operator = InsertOperator(exec_ctx, value_operator, "table0");

    EXPECT_EQ(RunOperator(insert_operator), std::vector<Tuple>{});

    std::vector<Tuple> results_table;
    auto &table = exec_ctx.catalog_.FetchTable("table0");
    auto read_guard = table.GetReadTableGuard();
    for (auto &row : read_guard.Rows()) {
        if (!row.tuple_meta_.is_deleted_) {
            results_table.push_back(row.tuple_);
        }
    }
    std::sort(results_table.begin(), results_table.end());
    EXPECT_EQ(results_table, insert_tuples);

    std::vector<Tuple> results_index;
    auto &index = exec_ctx.catalog_.FetchIndex("index0_0");
    for (data_t key = 0; key <= 5; key++) {
        auto row_id = index.LookupKey(key);
        if (row_id != INVALID_ID) {
            auto &row = read_guard.Rows()[row_id];
            if (!row.tuple_meta_.is_deleted_) {
                results_index.push_back(row.tuple_);
                EXPECT_EQ(results_index.back()[0], key);
            }
        }
    }
    std::sort(results_index.begin(), results_index.end());
    EXPECT_EQ(results_index, insert_tuples);

    read_guard.Drop();
    EXPECT_ANY_THROW(RunOperator(insert_operator));

    auto seq_scan_operator = SeqScanOperator(exec_ctx, "table0", {"c0", "c1"});
    EXPECT_EQ(RunOperator(seq_scan_operator), insert_tuples);

    auto range_index_scan_operator
        = RangeIndexScanOperator(exec_ctx, "table0", {"c0", "c1"}, {"c0", "c1"}, "index0_0", RangeInfo{1, 4});
    EXPECT_EQ(RunOperator(range_index_scan_operator), (std::vector<Tuple>{Tuple{2, 3}, Tuple{4, 5}}));

    auto write_guard = table.GetWriteTableGuard();
    for (auto &row : write_guard.Rows()) {
        row.tuple_meta_.is_deleted_ = true;
    }
    write_guard.Drop();

    EXPECT_EQ(RunOperator(seq_scan_operator), std::vector<Tuple>{});
    EXPECT_EQ(RunOperator(range_index_scan_operator), std::vector<Tuple>{});

    EXPECT_NO_THROW(RunOperator(insert_operator));

    test_db.Commit(*txn);
}

TEST(TableOperatorTest, DeleteBasicTest) {
    BabyDB test_db(TestConfig());
    Schema schema{"c0", "c1"};
    test_db.CreateTable("table0", schema);
    test_db.CreateIndex("index0_0", "table0", "c0", IndexType::Stlmap);
    auto txn = test_db.CreateTxn();
    auto exec_ctx = test_db.GetExecutionContext(txn);

    std::vector<Tuple> insert_tuples;
    insert_tuples.push_back({0, 1});
    insert_tuples.push_back({2, 3});
    insert_tuples.push_back({4, 5});

    auto value_operator = std::make_shared<ValueOperator>(exec_ctx, Schema{"c0", "c1"}, std::move(insert_tuples));
    auto insert_operator = InsertOperator(exec_ctx, value_operator, "table0");

    EXPECT_EQ(RunOperator(insert_operator), std::vector<Tuple>{});

    auto scan_operator = std::make_shared<SeqScanOperator>(exec_ctx, "table0");

    auto equal_filter = std::make_unique<EqualFilter>("table0.c0", 2);
    auto filter_operator = std::make_shared<FilterOperator>(exec_ctx, scan_operator, std::move(equal_filter));
    auto delete_operator = DeleteOperator(exec_ctx, filter_operator);

    EXPECT_EQ(RunOperator(delete_operator), std::vector<Tuple>{});
    EXPECT_EQ(RunOperator(*scan_operator), (std::vector<Tuple>{Tuple{0, 1}, Tuple{4, 5}}));

    test_db.Commit(*txn);
}

TEST(TableOperatorTest, UpdateScanBasicTest) {
    BabyDB test_db(TestConfig());
    Schema schema{"c0", "c1"};
    test_db.CreateTable("table0", schema);
    test_db.CreateIndex("index0_0", "table0", "c0", IndexType::Stlmap);
    auto txn = test_db.CreateTxn();
    auto exec_ctx = test_db.GetExecutionContext(txn);

    std::vector<Tuple> insert_tuples;
    insert_tuples.push_back({0, 1});
    insert_tuples.push_back({2, 3});
    insert_tuples.push_back({4, 5});

    auto value_operator = std::make_shared<ValueOperator>(exec_ctx, Schema{"c0", "c1"}, std::move(insert_tuples));
    auto insert_operator = InsertOperator(exec_ctx, value_operator, "table0");
    EXPECT_EQ(RunOperator(insert_operator), std::vector<Tuple>{});

    auto scan_operator = std::make_shared<SeqScanOperator>(exec_ctx, "table0");

    auto add_c1_projection = std::make_unique<UDProjection>("table0.c1", [](std::vector<data_t> &&input) {
        return input[0] + 1;
    });
    auto add_c1_projection_operator
        = std::make_shared<ProjectionOperator>(exec_ctx, scan_operator, std::move(add_c1_projection));
    auto add_c1_update_operator = UpdateOperator(exec_ctx, add_c1_projection_operator);

    EXPECT_EQ(RunOperator(add_c1_update_operator), std::vector<Tuple>{});
    EXPECT_EQ(RunOperator(*scan_operator), (std::vector<Tuple>{Tuple{0, 2}, Tuple{2, 4}, Tuple{4, 6}}));

    auto equal_filter = std::make_unique<EqualFilter>("table0.c0", 2);
    auto filter_operator = std::make_shared<FilterOperator>(exec_ctx, scan_operator, std::move(equal_filter));
    auto add_c0_projection = std::make_unique<UDProjection>("table0.c0", [](std::vector<data_t> &&input) {
        return input[0] * 3;
    });
    auto add_c0_projection_operator
        = std::make_shared<ProjectionOperator>(exec_ctx, filter_operator, std::move(add_c0_projection));
    auto add_c0_update_operator = UpdateOperator(exec_ctx, add_c0_projection_operator);

    EXPECT_EQ(RunOperator(add_c0_update_operator), std::vector<Tuple>{});
    EXPECT_EQ(RunOperator(*scan_operator), (std::vector<Tuple>{Tuple{0, 2}, Tuple{4, 6}, Tuple{6, 4}}));

    std::vector<Tuple> results_index;
    auto &table = exec_ctx.catalog_.FetchTable("table0");
    auto read_guard = table.GetReadTableGuard();
    auto &index = exec_ctx.catalog_.FetchIndex("index0_0");
    for (data_t key = 0; key <= 6; key++) {
        auto row_id = index.LookupKey(key);
        if (row_id != INVALID_ID) {
            auto &row = read_guard.Rows()[row_id];
            if (!row.tuple_meta_.is_deleted_) {
                results_index.push_back(row.tuple_);
                EXPECT_EQ(results_index.back()[0], key);
            }
        }
    }
    std::sort(results_index.begin(), results_index.end());
    EXPECT_EQ(results_index, (std::vector<Tuple>{Tuple{0, 2}, Tuple{4, 6}, Tuple{6, 4}}));

    test_db.Commit(*txn);
}

}