add_library(
    babydb_execution
    OBJECT
    expression/filter.cpp
    expression/projection.cpp
    aggregate_operator.cpp
    delete_operator.cpp
    execution_common.cpp
    hash_join_operator.cpp
    filter_operator.cpp
    seq_scan_operator.cpp
    insert_operator.cpp
    projection_operator.cpp
    range_index_scan_operator.cpp
    update_operator.cpp
    value_operator.cpp)

set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:babydb_execution>
    PARENT_SCOPE)
