#include "execution/aggregate_operator.hpp"

namespace babydb {

static Schema GetAggregateOutputSchema(const std::string &group_by, const std::string &aggregate_column) {
    Schema result;
    result.push_back(group_by);
    result.push_back("SUM(" + aggregate_column + ")");
    return result;
}

AggregateOperator::AggregateOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator,
                                     const std::string &group_by, const std::string &aggregate_column)
    : Operator(exec_ctx, {child_operator}, GetAggregateOutputSchema(group_by, aggregate_column)), group_by_(group_by),
      aggregate_column_(aggregate_column) {}

OperatorState AggregateOperator::Next(Chunk &output_chunk) {
    output_chunk.clear();
    if (!hash_table_build_) {
        hash_table_build_ = true;
        BuildHashTable();
        output_ptr_ = hash_table_.begin();
    }
    while (output_chunk.size() < exec_ctx_.config_.CHUNK_SUGGEST_SIZE) {
        if (output_ptr_ == hash_table_.end()) {
            return EXHAUSETED;
        }
        output_chunk.emplace_back(Tuple{output_ptr_->first, output_ptr_->second}, INVALID_ID);
        ++output_ptr_;
    }
    return HAVE_MORE_OUTPUT;
}

void AggregateOperator::SelfInit() {
    hash_table_.clear();
    hash_table_build_ = false;
    output_ptr_ = hash_table_.end();
}

void AggregateOperator::SelfCheck() {
    auto &input_schema = child_operators_[0]->GetOutputSchema();
    input_schema.GetKeyAttr(group_by_);
    input_schema.GetKeyAttr(aggregate_column_);
}

void AggregateOperator::BuildHashTable() {
    Chunk input_chunk;
    auto child_state = OperatorState::HAVE_MORE_OUTPUT;

    auto &input_schema = child_operators_[0]->GetOutputSchema();
    auto group_by_attr = input_schema.GetKeyAttr(group_by_);
    auto aggregate_column_attr = input_schema.GetKeyAttr(aggregate_column_);

    while (child_state != EXHAUSETED) {
        child_state = child_operators_[0]->Next(input_chunk);
        for (auto &data : input_chunk) {
            auto &tuple = data.first;
            auto group_by_key = tuple.KeyFromTuple(group_by_attr);
            auto aggregate_value = tuple.KeyFromTuple(aggregate_column_attr);
            auto table_ite = hash_table_.find(group_by_key);
            if (table_ite == hash_table_.end()) {
                hash_table_.insert(std::make_pair(group_by_key, aggregate_value));
            } else {
                table_ite->second += aggregate_value;
            }
        }
    }
}

}