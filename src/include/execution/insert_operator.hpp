#pragma once

#include "execution/operator.hpp"

#include <string>

namespace babydb {

/**
 * Insert Operator
 * By default, it will use table's schema to parse the input.
 * You can also specify how to parse the input.
 */
class InsertOperator : public Operator {
public:
    InsertOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator,
                   const std::string &table_name);

    InsertOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator,
                   const std::string &table_name, const Schema &input_schema);

    ~InsertOperator() override = default;

    OperatorState Next(Chunk &) override;

    void SelfInit() override {}

    void SelfCheck() override;

    std::string Type() override;

private:
    std::string table_name_;

    Schema input_schema_;
};

}