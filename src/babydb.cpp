#include <cmath>
#include <iostream>

#include "babydb.hpp"

#include "common/typedefs.hpp"
#include "execution/hash_join_operator.hpp"
#include "execution/seq_scan_operator.hpp"
#include "execution/value_operator.hpp"
#include "storage/catalog.hpp"
#include "storage/index.hpp"
#include "storage/stlmap_index.hpp"
#include "storage/table.hpp"
#include "concurrency/transaction_manager.hpp"

namespace babydb {

BabyDB::BabyDB(const ConfigGroup &config) : catalog_(std::make_unique<Catalog>()),
    txn_mgr_(std::make_unique<TransactionManager>()), config_(std::make_unique<ConfigGroup>(config)) {}

BabyDB::~BabyDB() {
    catalog_.reset();
    txn_mgr_.reset();
}

void BabyDB::CreateTable(const std::string &table_name, const Schema &schema) {
    std::unique_lock lock(db_lock_);
    catalog_->CreateTable(std::make_unique<Table>(table_name, schema));
}

void BabyDB::DropTable(const std::string &table_name) {
    std::unique_lock lock(db_lock_);
    catalog_->DropTable(table_name);
}

void BabyDB::CreateIndex(const std::string &index_name, const std::string &table_name, const std::string &key_column,
                         IndexType index_type) {
    std::unique_lock lock(db_lock_);
    auto &table = catalog_->FetchTable(table_name);

    switch (index_type) {
    case Stlmap:
        catalog_->CreateIndex(std::make_unique<StlmapIndex>(index_name, table, key_column));
        break;
    
    default:
        throw std::logic_error("CREATE INDEX: unknown index type");
    }
}

void BabyDB::DropIndex(const std::string &index_name) {
    std::unique_lock lock(db_lock_);
    catalog_->DropIndex(index_name);
}

std::shared_ptr<Transaction> BabyDB::CreateTxn() {
    return txn_mgr_->CreateTxn(std::unique_lock(db_lock_));
}

bool BabyDB::Commit(Transaction &txn) {
    return txn_mgr_->Commit(txn);
}

void BabyDB::Abort(Transaction &txn) {
    txn_mgr_->Abort(txn);
}

struct joinfo {
    std::string to, col, to_col;
    joinfo(std::string x, std::string y, std::string z) : to(x), col(y), to_col(z) {}
};
std::unordered_map<std::string, std::shared_ptr<ValueOperator>> vp;
void pre(std::string table, const ExecutionContext &exec_ctx) {
    if (vp.find(table) != vp.end()) return;
    auto seqscan = std::make_shared<SeqScanOperator>(exec_ctx, table);
    std::vector<Tuple> tuples;
    OperatorState state = OperatorState::HAVE_MORE_OUTPUT;
    Chunk data_chunk;
    while (state == OperatorState::HAVE_MORE_OUTPUT) {
        state = seqscan->Next(data_chunk);
        for (auto &[tuple, meta] : data_chunk) {
            tuples.emplace_back(tuple);
        }
    }
    // std::cout << table << " " << tuples.size() << std::endl;
    vp[table] = std::make_shared<ValueOperator>(exec_ctx, seqscan->GetOutputSchema(), std::move(tuples));
}
std::unordered_multimap<std::string, joinfo> edge;
void buildtree(const std::shared_ptr<Operator> &node, const ExecutionContext &exec_ctx) {
    if(node->Type() != "HashJoinOperator") {
        return;
    }
    auto hnode = std::dynamic_pointer_cast<HashJoinOperator>(node);
    buildtree(hnode->GetChildOperators()[0], exec_ctx);
    buildtree(hnode->GetChildOperators()[1], exec_ctx);
    std::string u = hnode->BuildTableId(), ucol = hnode->BuildTableCol();
    std::string v = hnode->ProbeTableId(), vcol = hnode->ProbeTableCol();
    pre(u, exec_ctx), pre(v, exec_ctx);
    edge.insert(std::make_pair(u, joinfo(v, ucol, vcol)));
    edge.insert(std::make_pair(v, joinfo(u, vcol, ucol)));
}
struct BloomFilter {
    idx_t hash_num;
    const double ln2 = 0.69314718056, lnp = -4.60517018599;// p = 0.01
    std::string table_name, col_name;
    std::vector<bool> bitmap;
    BloomFilter(std::string t, std::string c) : table_name(t), col_name(c) {
        bitmap.clear();
        hash_num = 0;
    }
    idx_t murmurhash3_64(data_t key, idx_t seed = 0) {
        key ^= seed;
        key ^= key >> 33;
        key *= 0xff51afd7ed558ccdULL;
        key ^= key >> 33;
        key *= 0xc4ceb9fe1a85ec53ULL;
        key ^= key >> 33;
        return key;
    }
    void insert(data_t x) {
        idx_t mod = bitmap.size();
        for (idx_t i = 0; i < hash_num; i++) {
            data_t y = murmurhash3_64(x, i) & (mod - 1);
            bitmap.at(y) = true;
        }
    }
    bool test(data_t x) {
        idx_t mod = bitmap.size();
        for (idx_t i = 0; i < hash_num; i++) {
            data_t y = murmurhash3_64(x, i) & (mod - 1);
            if (!bitmap.at(y)) return false;
        }
        return true;
    }
    void build() {
        std::vector<data_t> data;
        auto &value_op = vp[table_name];
        OperatorState state = OperatorState::HAVE_MORE_OUTPUT;
        Chunk data_chunk;
        while (state == OperatorState::HAVE_MORE_OUTPUT) {
            state = value_op->Next(data_chunk);
            for (auto &[tuple, meta] : data_chunk) {
                data.emplace_back(tuple.KeyFromTuple(std::stoull(col_name)));
            }
        }
        value_op->Reset();
        idx_t n = data.size();
        idx_t m = 1 << static_cast<int>(std::round(std::log2(-lnp * n / ln2 / ln2)));
        hash_num = std::round(m * ln2 / n);
        // std::cout << "???" << m << " " << hash_num << std::endl;
        bitmap.resize(m, false);
        for (data_t x : data) insert(x);
    }
};
void semi_join(std::string table, std::string colt, std::string filter, std::string colf, const ExecutionContext &exec_ctx) {
    auto bf = BloomFilter(filter, colf);
    bf.build();
    auto &value_op = vp[table];
    std::vector<Tuple> tuples;
    OperatorState state = OperatorState::HAVE_MORE_OUTPUT;
    Chunk data_chunk;
    while (state == OperatorState::HAVE_MORE_OUTPUT) {
        state = value_op->Next(data_chunk);
        for (auto &[tuple, meta] : data_chunk) {
            auto key = tuple.KeyFromTuple(std::stoull(colt));
            if (!bf.test(key)) continue;
            tuples.push_back(tuple);
        }
    }
    value_op = std::make_shared<ValueOperator>(exec_ctx, value_op->GetOutputSchema(), std::move(tuples));
}
void forwardpass(std::string u, std::string fa, const ExecutionContext &exec_ctx) {
    auto range = edge.equal_range(u);
    for (auto it = range.first; it != range.second; it++) {
        std::string v = it->second.to;
        if(v == fa) continue;
        forwardpass(v, u, exec_ctx);
        semi_join(u, it->second.col, v, it->second.to_col, exec_ctx);
    }
}
void backwardpass(std::string u, std::string fa, const ExecutionContext &exec_ctx) {
    auto range = edge.equal_range(u);
    for (auto it = range.first; it != range.second; it++) {
        std::string v = it->second.to;
        if(v == fa) continue;
        semi_join(v, it->second.to_col, u, it->second.col, exec_ctx);
        backwardpass(v, u, exec_ctx);
    }
}
void optimize(std::shared_ptr<Operator> &node) {
    if(node->Type() != "HashJoinOperator") {
        auto snode = std::dynamic_pointer_cast<SeqScanOperator>(node);
        node = vp[snode->BindTableName()];
        return;
    }
    optimize(node->GetChildOperators()[0]);
    optimize(node->GetChildOperators()[1]);
}
void BabyDB::OptimizeJoinPlan(std::shared_ptr<Operator> &join_plan) {
    vp.clear();
    edge.clear();
    auto exec_ctx = join_plan->GetExecutionContext();
    buildtree(join_plan, exec_ctx);
    std::string root = edge.begin()->first;
    forwardpass(root, "", exec_ctx);
    backwardpass(root, "", exec_ctx);
    optimize(join_plan);
}

}