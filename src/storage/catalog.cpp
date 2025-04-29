#include "storage/catalog.hpp"

#include "storage/index.hpp"
#include "storage/table.hpp"

#include <mutex>

namespace babydb {

void Catalog::CreateTable(std::unique_ptr<Table> table) {
    if (tables_.find(table->name_) != tables_.end()) {
        throw std::logic_error("CREATE TABLE: table already exists");
    }
    tables_.insert(std::make_pair(table->name_, std::move(table)));
}

void Catalog::DropTable(const std::string &table_name) {
    auto position = tables_.find(table_name);
    if (position == tables_.end()) {
        throw std::logic_error("DROP TABLE: table does not exist");
    }
    if (position->second->index_name_ != INVALID_NAME) {
        indexes_.erase(position->second->index_name_);
    }
    tables_.erase(position);
}

void Catalog::CreateIndex(std::unique_ptr<Index> index) {
    if (indexes_.find(index->name_) != indexes_.end()) {
        throw std::logic_error("CREATE INDEX: index already exists");
    }
    auto table_position = tables_.find(index->table_name_);
    if (table_position == tables_.end()) {
        throw std::logic_error("CREATE INDEX: table does not exist");
    }
    if (table_position->second->index_name_ != INVALID_NAME) {
        throw std::logic_error("CREATE INDEX: table already has an index");
    }
    table_position->second->index_name_ = index->name_;
    indexes_.insert(std::make_pair(index->name_, std::move(index)));
}

void Catalog::DropIndex(const std::string &index_name) {
    auto position = indexes_.find(index_name);
    if (position == indexes_.end()) {
        throw std::logic_error("DROP INDEX: index does not exist");
    }
    auto table_position = tables_.find(position->second->table_name_);
    table_position->second->index_name_ = INVALID_NAME;
    indexes_.erase(position);
}

Table& Catalog::FetchTable(const std::string &table_name) const {
    auto position = tables_.find(table_name);
    if (position == tables_.end()) {
        throw std::logic_error("Fetch table: table does not exist");
    }
    return *position->second;
}

Index& Catalog::FetchIndex(const std::string &index_name) const {
    auto position = indexes_.find(index_name);
    if (position == indexes_.end()) {
        throw std::logic_error("Fetch table: index does not exist");
    }
    return *position->second;
}

}