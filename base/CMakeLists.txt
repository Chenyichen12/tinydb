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
sql/executor.h
sql/executor.cpp
sql/seqexecutor.h
sql/seqexecutor.cpp
sql/filterexecutor.h
sql/filterexecutor.cpp
)

target_include_directories(basedb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(basedb PRIVATE sqlparser nlohmann_json::nlohmann_json)

target_compile_options(basedb PRIVATE -finput-charset=UTF-8)
