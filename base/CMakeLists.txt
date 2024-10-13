project(basedb)

add_executable(basedb main.cpp)
target_link_libraries(basedb PRIVATE sqlparser)