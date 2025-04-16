#include "execution/delete_operator.hpp"

#include "execution/execution_common.hpp"
#include "storage/catalog.hpp"
#include "storage/table.hpp"

namespace babydb {

DeleteOperator::DeleteOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator)
    : Operator(exec_ctx, {child_operator}, Schema{}), table_name_(child_operator->BindTableName()) {}

void DeleteOperator::SelfCheck() {
    exec_ctx_.catalog_.FetchTable(table_name_);
}

OperatorState DeleteOperator::Next(Chunk &output_chunk) {
    output_chunk.clear();
    auto &table = exec_ctx_.catalog_.FetchTable(table_name_);

    Chunk delete_chunk;
    auto child_state = OperatorState::HAVE_MORE_OUTPUT;
    while (child_state != EXHAUSETED) {
        child_state = child_operators_[0]->Next(delete_chunk);
        auto write_guard = table.GetWriteTableGuard();
        for (auto &delete_data : delete_chunk) {
            B_ASSERT(delete_data.second != INVALID_ID);
            DeleteRow(write_guard, delete_data.second);
        }
    }
    return EXHAUSETED;
}

}