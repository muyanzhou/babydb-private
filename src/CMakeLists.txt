add_subdirectory(concurrency)
add_subdirectory(execution)
add_subdirectory(storage)

add_library(babydb
        STATIC
        babydb.cpp
        ${ALL_OBJECT_FILES})

set(BABYDB_LIBS
        babydb_concurrency
        babydb_execution
        babydb_storage
        )

target_link_libraries(
        babydb
        ${BABYDB_LIBS})

target_include_directories(
        babydb PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
