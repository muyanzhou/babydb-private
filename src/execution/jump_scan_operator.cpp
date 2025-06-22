#include <iostream>
#include "execution/jump_scan_operator.hpp"

#include "storage/catalog.hpp"
#include "storage/table.hpp"

namespace babydb {

static const Schema& FetchTableSchema(const ExecutionContext &exec_ctx, const std::string &table_name) {
    auto &table = exec_ctx.catalog_.FetchTable(table_name);
    return table.schema_;
}

static Schema CombineSchema(const std::string &table_name, const Schema &schema) {
    auto schema_copy = schema;
    for (auto &column : schema_copy) {
        column = table_name + "." + column;
    }
    return schema_copy;
}

JumpScanOperator::JumpScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name)
    : JumpScanOperator(exec_ctx, table_name, FetchTableSchema(exec_ctx, table_name)) {}

JumpScanOperator::JumpScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name,
                                 const Schema &fetch_columns)
    : JumpScanOperator(exec_ctx, table_name, fetch_columns, table_name) {}

JumpScanOperator::JumpScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name,
                                 const Schema &fetch_columns, const std::string &table_output_name)
    : JumpScanOperator(exec_ctx, table_name, fetch_columns, CombineSchema(table_output_name, fetch_columns)) {}

JumpScanOperator::JumpScanOperator(const ExecutionContext &exec_ctx, const std::string &table_name,
                                 const Schema &fetch_columns, const Schema &output_schema)
    : Operator(exec_ctx, {}, output_schema), table_name_(table_name), fetch_columns_(fetch_columns) {}

JumpScanOperator::~JumpScanOperator() {
    cur = head;
    while (cur != tail) {
        cur = cur->nxt;
        delete cur->pre;
    }
    delete cur;
}

OperatorState JumpScanOperator::Next(Chunk &output_chunk) {
    output_chunk.resize(exec_ctx_.config_.CHUNK_SUGGEST_SIZE);
    idx_t output_size = 0;

    auto &table = exec_ctx_.catalog_.FetchTable(table_name_);
    auto key_attrs = table.schema_.GetKeyAttrs(fetch_columns_);

    auto read_guard = table.GetReadTableGuard();

    if (!default_) {
        idx_t size = read_guard.Rows().size();
        cur = head;
        for (idx_t i = 0; i < size; i++) {
            auto now = new node(i, cur, nullptr);
            cur->nxt = now;
            cur = now;
        }
        cur->nxt = tail;
        tail->pre = cur;
        cur = head->nxt;
        default_ = true;
    }

    while (output_size < exec_ctx_.config_.CHUNK_SUGGEST_SIZE) {
        if (cur == tail) {
            output_chunk.resize(output_size);
            return EXHAUSETED;
        }
        idx_t id = cur->row_id;
        auto& [tuple, meta] = read_guard.Rows()[id];
        cur = cur->nxt;

        if (meta.is_deleted_) {
            continue;
        }
        output_chunk[output_size].first = tuple.KeysFromTuple(key_attrs);
        output_chunk[output_size].second = id;
        output_size++;
    }
    output_chunk.resize(output_size);
    return HAVE_MORE_OUTPUT;
}

void JumpScanOperator::Delete(std::vector<idx_t> &del) {
    auto link=[](node* a, node* b) {
        a->nxt = b;
        b->pre = a;
    };
    auto tmp = cur;
    // std::cout << tmp->row_id << std::endl;
    while (!del.empty()) {
        idx_t now = del.back();
        while (tmp == tail || tmp->row_id > now) tmp = tmp->pre;
        if (tmp->row_id != now) {
            // std::cout << now << " " << tmp->row_id << std::endl;
            throw std::logic_error("JumpScanOperator: Fail to delete");
        }
        auto ptr = tmp;
        link(tmp->pre, tmp->nxt);
        tmp = tmp->pre;
        delete ptr;
        del.pop_back();
    }
}

void JumpScanOperator::SelfInit() {
    default_ = false;
    head = new node(0, nullptr, nullptr);
    tail = new node(0, nullptr, nullptr);
    cur = head;
}

void JumpScanOperator::SelfCheck() {
    FetchTableSchema(exec_ctx_, table_name_).GetKeyAttrs(fetch_columns_);

    if (fetch_columns_.size() != output_schema_.size()) {
        throw std::logic_error("JumpScanOperator: Fetch columns and output schema do not match");
    }
}

}