#include "gtest/gtest.h"

#include "babydb.hpp"
#include "storage/table.hpp"

#include <thread>

namespace babydb {

TEST(TableTest, BasicTest) {
    Schema schema{"c0", "c1"};
    Table table("table", schema);

    auto write_guard = table.GetWriteTableGuard();
    write_guard.Rows().push_back(Row{Tuple{0, 1}, TupleMeta()});
    write_guard.Rows().push_back(Row{Tuple{2, 3}, TupleMeta()});
    write_guard.Drop();

    auto read_guard_0 = table.GetReadTableGuard();
    auto read_guard_1 = table.GetReadTableGuard();

    EXPECT_EQ(read_guard_0.Rows()[0].tuple_, (Tuple{0, 1}));
    EXPECT_EQ(read_guard_0.Rows()[0].tuple_meta_.is_deleted_, false);
}

TEST(TableTest, ConcurrentTest) {
    Schema schema{"c0", "c1"};
    Table table("table", schema);

    const idx_t thread_count = 4;
    const idx_t operation_count = 5000;

    std::vector<std::thread> thread_list;

    for (idx_t thread_id = 0; thread_id < thread_count; thread_id++) {
        thread_list.emplace_back([&table](idx_t start_id, idx_t end_id) {
            for (idx_t element_id = start_id; element_id < end_id; element_id++) {
                data_t val = static_cast<data_t>(element_id);
                auto write_guard = table.GetWriteTableGuard();
                auto before_length = write_guard.Rows().size();
                write_guard.Rows().push_back(Row{Tuple{val, val * 2}, TupleMeta()});
                write_guard.Drop();

                auto read_guard = table.GetReadTableGuard();
                auto cur_length = read_guard.Rows().size();
                EXPECT_GT(cur_length, before_length);
                EXPECT_EQ(read_guard.Rows()[before_length].tuple_[0], element_id);
            }
        }, thread_id * operation_count, (thread_id + 1) * operation_count);
    }
    for (auto &thread_item : thread_list) {
        thread_item.join();
    }

    auto read_guard = table.GetReadTableGuard();
    EXPECT_EQ(read_guard.Rows().size(), operation_count * thread_count);
    int64_t expected_sum2 = 0;
    int64_t sum2 = 0;
    for (idx_t i = 0; i < read_guard.Rows().size(); i++) {
        expected_sum2 += i * i;
        sum2 += read_guard.Rows()[i].tuple_[0] * read_guard.Rows()[i].tuple_[0];
    }
    EXPECT_EQ(expected_sum2, sum2);
}

}