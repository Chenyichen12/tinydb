project(basedb)

add_executable(basedb main.cpp
file/store.cpp
file/store.h
database.h
database.cpp
table.h
table.cpp

sql/runner.h
sql/runner.cpp
executor/executor.h
executor/executor.cpp
executor/seqexecutor.h
executor/seqexecutor.cpp
executor/filterexecutor.h
executor/filterexecutor.cpp
executor/limitexecutor.h
executor/limitexecutor.cpp
executor/orderexecutor.h
executor/orderexecutor.cpp
)

target_include_directories(basedb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(basedb PRIVATE sqlparser nlohmann_json::nlohmann_json)

target_compile_options(basedb PRIVATE -finput-charset=UTF-8)
