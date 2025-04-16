#pragma once

#include "common/typedefs.hpp"
#include "common/macro.hpp"

#include <map>
#include <memory>
#include <shared_mutex>
#include <string>

namespace babydb {

class Table;
class Index;

//! Catalog is not thread-safe. Since it's updated infrequently, when update the catalog,
//! you should hold the database's latch.
class Catalog {
public:
    Catalog() = default;

    DISALLOW_COPY_AND_MOVE(Catalog);

    void CreateTable(std::unique_ptr<Table> table);

    void DropTable(const std::string &table_name);

    void CreateIndex(std::unique_ptr<Index> index);

    void DropIndex(const std::string &index_name);

    Table& FetchTable(const std::string &table_name) const;

    Index& FetchIndex(const std::string &index_name) const;

private:
    std::map<std::string, std::unique_ptr<Table>> tables_;

    std::map<std::string, std::unique_ptr<Index>> indexes_;
};

}