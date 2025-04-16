#pragma once

#include "execution/operator.hpp"

#include <string>

namespace babydb {

/**
 * Delete Operator
 * It will delete rows by input's row id.
 */
class DeleteOperator : public Operator {
public:
    DeleteOperator(const ExecutionContext &exec_ctx, const std::shared_ptr<Operator> &child_operator);

    ~DeleteOperator() override = default;

    OperatorState Next(Chunk &) override;

    void SelfInit() override {}

    void SelfCheck() override;

private:
    std::string table_name_;
};

}