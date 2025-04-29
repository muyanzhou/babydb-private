#include "execution/value_operator.hpp"

namespace babydb {

ValueOperator::ValueOperator(const ExecutionContext &execute_context, const Schema &output_schema, std::vector<Tuple> &&tuples)
    : Operator(execute_context, {}, output_schema), tuples_(std::move(tuples)) {}

void ValueOperator::SelfInit() {
    next_tuple_id = 0;
}

void ValueOperator::SelfCheck() {
    for (auto &tuple : tuples_) {
        if (output_schema_.size() != tuple.size()) {
            throw std::logic_error("Invalid schema or tuple for ValueOperator");
        }
    }
}

OperatorState ValueOperator::Next(Chunk &output_chunk) {
    output_chunk.clear();
    while (output_chunk.size() < exec_ctx_.config_.CHUNK_SUGGEST_SIZE) {
        if (next_tuple_id == tuples_.size()) {
            return EXHAUSETED;
        }
        output_chunk.emplace_back(tuples_[next_tuple_id], INVALID_ID);
        next_tuple_id++;
    }
    return HAVE_MORE_OUTPUT;
}

}