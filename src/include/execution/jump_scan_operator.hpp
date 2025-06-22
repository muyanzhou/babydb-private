#pragma once

#include "execution/operator.hpp"

namespace babydb {

class JumpScanOperator : public Operator {
public:
    JumpScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name);

    JumpScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name, const Schema &fetch_columns);

    JumpScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name, const Schema &fetch_columns,
                    const std::string &table_output_name);

    JumpScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name, const Schema &fetch_columns,
                    const Schema &output_schema);

    ~JumpScanOperator() override;

    OperatorState Next(Chunk &output_chunk) override;

    void SelfInit() override;

    void SelfCheck() override;

    std::string Type() override { return "JumpScanOperator"; }

    std::string BindTableName() override { return table_name_; }

    void Reset() override { cur = head->nxt; }

    void Delete(std::vector<idx_t> &del);

private:
    std::string table_name_;

    Schema fetch_columns_;

    bool default_{false};

    struct node {
        idx_t row_id;
        node *nxt, *pre;
        node(idx_t rd, node* ptr1, node* ptr2) : row_id(rd), pre(ptr1), nxt(ptr2) {}
    };
    node *head{nullptr}, *tail{nullptr}, *cur{nullptr};
};

}