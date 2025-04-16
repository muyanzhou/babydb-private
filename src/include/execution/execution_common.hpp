#include "common/typedefs.hpp"

namespace babydb {

class Index;
class WriteTableGuard;

//! Remove a row from a table
void DeleteRow(WriteTableGuard &write_guard, idx_t row_id);
//! Insert to a table with a index
void InsertRowWithIndex(WriteTableGuard &write_guard, Tuple &&tuple, Index *index, const data_t &key);
//! Insert to a table WITHOUT index
void InsertRowWoIndex(WriteTableGuard &write_guard, Tuple &&tuple);

}