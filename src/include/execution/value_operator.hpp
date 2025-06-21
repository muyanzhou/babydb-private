#pragma once

#include "execution/operator.hpp"

namespace babydb {

/**
 * Value Operator
 * It will output rows stored in the operator.
 */
class ValueOperator : public Operator {
public:
    ValueOperator(const ExecutionContext &execute_context, const Schema &output_schema, std::vector<Tuple> &&tuples);

    ~ValueOperator() override = default;

    void SelfInit() override;

    void SelfCheck() override;

    std::string Type() override;

    OperatorState Next(Chunk &output_chunk) override;

    idx_t Size() {return tuples_.size();}

    void Reset() {next_tuple_id = 0;}

private:
    std::vector<Tuple> tuples_;

    idx_t next_tuple_id{0};
};

}