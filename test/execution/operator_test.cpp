#include "gtest/gtest.h"

#include "babydb.hpp"
#include "execution/aggregate_operator.hpp"
#include "execution/hash_join_operator.hpp"
#include "execution/filter_operator.hpp"
#include "execution/projection_operator.hpp"
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

TEST(OperatorTest, ValueBasicTest) {
    BabyDB test_db(TestConfig());
    auto txn = test_db.CreateTxn();
    auto exec_ctx = test_db.GetExecutionContext(txn);
    std::vector<Tuple> tuples;
    tuples.push_back({0, 1});
    tuples.push_back({2, 3});
    tuples.push_back({4, 5});
    tuples.push_back({6, 7});
    auto tuples_copy = tuples;
    ValueOperator test_operator(exec_ctx, {"c0", "c1"}, std::move(tuples_copy));

    EXPECT_EQ(RunOperator(test_operator), tuples);
    test_db.Commit(*txn);
}

TEST(OperatorTest, HashJoinBasicTest) {
    BabyDB test_db(TestConfig());
    auto txn = test_db.CreateTxn();
    auto exec_ctx = test_db.GetExecutionContext(txn);

    std::vector<Tuple> build_tuples;
    build_tuples.push_back({0, 1});
    build_tuples.push_back({2, 3});
    build_tuples.push_back({2, 4});
    build_tuples.push_back({4, 5});

    std::vector<Tuple> probe_tuples;
    probe_tuples.push_back({0, 0});
    probe_tuples.push_back({2, 2});
    probe_tuples.push_back({3, 3});

    std::vector<Tuple> answers;
    answers.push_back({0, 0, 0, 1});
    answers.push_back({2, 2, 2, 3});
    answers.push_back({2, 2, 2, 4});

    auto build_operator = std::make_shared<ValueOperator>(exec_ctx, Schema{"b0", "b1"}, std::move(build_tuples));
    auto probe_operator = std::make_shared<ValueOperator>(exec_ctx, Schema{"p0", "p1"}, std::move(probe_tuples));
    auto test_operator = HashJoinOperator(exec_ctx, probe_operator, build_operator, "p0", "b0");

    EXPECT_EQ(RunOperator(test_operator), answers);
    test_db.Commit(*txn);
}

TEST(OperatorTest, FilterTest) {
    BabyDB test_db(TestConfig());
    auto txn = test_db.CreateTxn();
    auto exec_ctx = test_db.GetExecutionContext(txn);

    std::vector<Tuple> tuples;
    tuples.push_back({0, 1});
    tuples.push_back({2, 3});
    tuples.push_back({4, 4});

    auto value_operator = std::make_shared<ValueOperator>(exec_ctx, Schema{"c0", "c1"}, std::move(tuples));

    auto equal_filter = std::make_unique<EqualFilter>("c0", 0);
    auto equal_filter_operator = FilterOperator(exec_ctx, value_operator, std::move(equal_filter));
    EXPECT_EQ(RunOperator(equal_filter_operator), (std::vector<Tuple>{Tuple{0, 1}}));

    auto range_filter = std::make_unique<RangeFilter>("c0", RangeInfo{1, 4});
    auto range_filter_operator = FilterOperator(exec_ctx, value_operator, std::move(range_filter));
    EXPECT_EQ(RunOperator(range_filter_operator), (std::vector<Tuple>{Tuple{2, 3}, Tuple{4, 4}}));

    auto udfilter = std::make_unique<UDFilter>(Schema{"c0", "c1"}, [](Tuple &&keys) {
        return keys[0] == keys[1];
    });
    auto udfilter_operator = FilterOperator(exec_ctx, value_operator, std::move(udfilter));
    EXPECT_EQ(RunOperator(udfilter_operator), (std::vector<Tuple>{Tuple{4, 4}}));
}

TEST(OperatorTest, ProjectionTest) {
    BabyDB test_db(TestConfig());
    auto txn = test_db.CreateTxn();
    auto exec_ctx = test_db.GetExecutionContext(txn);

    std::vector<Tuple> tuples;
    tuples.push_back({0, 1});
    tuples.push_back({2, 3});
    tuples.push_back({4, 5});

    auto value_operator = std::make_shared<ValueOperator>(exec_ctx, Schema{"c0", "c1"}, std::move(tuples));

    auto add_one_projection = std::make_unique<UDProjection>("c1", [](std::vector<data_t> &&input) {
        return input[0] + 1;
    });
    auto add_one_projection_operator = ProjectionOperator(exec_ctx, value_operator, std::move(add_one_projection));
    EXPECT_EQ(RunOperator(add_one_projection_operator), (std::vector<Tuple>{Tuple{0, 2}, Tuple{2, 4}, Tuple{4, 6}}));

    auto sum_projection = std::make_unique<UDProjection>(Schema{"c0", "c1"}, "sum", [](std::vector<data_t> &&input) {
        return input[0] + input[1];
    });
    auto sum_projection_operator = ProjectionOperator(exec_ctx, value_operator, std::move(sum_projection), false);
    EXPECT_EQ(RunOperator(sum_projection_operator), (std::vector<Tuple>{Tuple{1}, Tuple{5}, Tuple{9}}));
}

TEST(OperatorTest, AggregateBasicTest) {
    BabyDB test_db(TestConfig());
    auto txn = test_db.CreateTxn();
    auto exec_ctx = test_db.GetExecutionContext(txn);

    std::vector<Tuple> test_tuples;
    test_tuples.push_back({0, 1});
    test_tuples.push_back({2, 3});
    test_tuples.push_back({2, 4});
    test_tuples.push_back({4, 5});
    test_tuples.push_back({4, 5});

    std::vector<Tuple> answers;
    answers.push_back({0, 1});
    answers.push_back({2, 7});
    answers.push_back({4, 10});

    auto value_operator = std::make_shared<ValueOperator>(exec_ctx, Schema{"c0", "c1"}, std::move(test_tuples));
    auto test_operator = AggregateOperator(exec_ctx, value_operator, "c0", "c1");

    EXPECT_EQ(RunOperator(test_operator), answers);
    test_db.Commit(*txn);
}

}