#include "gtest/gtest.h"
#include "storage/art.hpp"

namespace babydb {

TEST(ArtIndexTest, BasicTest) {
    const idx_t N = 1000;
    Schema schema{"c0", "c1"};
    Table table("table", schema);
    {
        auto write_guard = table.GetWriteTableGuard();
        for (idx_t i = 0; i < N; i++) {
            write_guard.Rows().push_back({Tuple{i, i + 100}, TupleMeta()});
        }
    }
    ArtIndex index("art_index", table, "c0");

    for (idx_t i = 0; i < N; i += 100) {
        EXPECT_EQ(index.LookupKey(i), i);
    }
}

} // namespace babydb
