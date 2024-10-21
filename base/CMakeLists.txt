project(basedb)

add_executable(basedb main.cpp
file/store.cpp
file/store.h
database.h
database.cpp
table.h
table.cpp
)
target_link_libraries(basedb PRIVATE sqlparser nlohmann_json::nlohmann_json)

target_compile_options(basedb PRIVATE -finput-charset=UTF-8)
