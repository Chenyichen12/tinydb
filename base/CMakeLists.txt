project(basedb)

add_executable(basedb main.cpp
file/store.cpp
file/store.h
)
target_link_libraries(basedb PRIVATE sqlparser)