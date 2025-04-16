#pragma once

namespace babydb {

class Transaction;
class Catalog;
class ConfigGroup;
struct ExecutionContext {
    Transaction &txn_;
    const Catalog &catalog_;
    const ConfigGroup &config_;
};

}