add_library(
    babydb_storage
    OBJECT
    catalog.cpp
    index.cpp
    stlmap_index.cpp
    art.cpp
    table.cpp)

set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:babydb_storage>
    PARENT_SCOPE)
