#pragma once

#include "common/typedefs.hpp"
#include "execution/operator.hpp"

#include <optional>

namespace babydb {

/**
 * Update Operator
 * Update the table by row id.
 * By default, it will direct use the input data.
 * The input schema can also be specified.
 */
class UpdateOperator : public Operator {
public:
    UpdateOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator);

    UpdateOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator,
                   const Schema &input_schema);

    ~UpdateOperator() override = default;

    OperatorState Next(Chunk &) override;

    void SelfInit() override {}

    void SelfCheck() override;

private:
    std::string table_name_;

    std::optional<Schema> input_schema_;
};

}