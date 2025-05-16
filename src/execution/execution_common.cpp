#include "execution/execution_common.hpp"

#include "storage/index.hpp"
#include "storage/table.hpp"
#include "storage/catalog.hpp"
#include "execution/execution_context.hpp"
#include "concurrency/transaction.hpp"
#include "concurrency/version_link.hpp"
#include <iostream>

namespace babydb {

void InsertRow(WriteTableGuard &write_guard, Tuple &&tuple, Index *index, const data_t &key, ExecutionContext &exec_ctx) {
    idx_t rid = index->LookupKey(key, exec_ctx);
    if (rid == INVALID_ID) {
        //! Insert a new row, used when initially inserting a tuple.
        rid = write_guard.Rows().size();
        index->InsertEntry(key, rid, exec_ctx);
        auto table = &exec_ctx.catalog_.FetchTable(index->table_name_);
        write_guard.Rows().emplace_back(exec_ctx.txn_, tuple, rid, table);
        exec_ctx.txn_.AddModifiedRow(rid, table);
        return;
    }
    write_guard.Rows()[rid].tryInsert(tuple, exec_ctx.txn_);
    exec_ctx.txn_.AddModifiedRow(rid, &exec_ctx.catalog_.FetchTable(index->table_name_));
}

}