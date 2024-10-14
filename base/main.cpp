
#if false
#include "file/store.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <unistd.h>
const char *PATH = "./test.db";
#define COUNT 100

void callbakc(void *key, void *value) {
  printf("key: %d value: %s\n", *(int*)key, (char *)value);
}
int main() {

  db_t *db;
  int i, rc;
  char value[128];

  // 创建数据库
  unlink(PATH);
  assert(db_create(PATH, DB_INT32KEY, sizeof(int)) == 0);

  // 打开数据库
  assert(db_open(&db, PATH) == 0);

  // 插入操作
  for (i = 0; i < COUNT; i++) {
    sprintf(value, "%d", i + 100);
    assert(db_insert(db, &i, value, strlen(value)) == 1);
  }
  printf("insert key from %d to %d\n", 0, COUNT);

  // 查询操作
  i = 10;
  bzero(value, sizeof(value));
  rc = db_search(db, &i, value, 1000);
  assert(rc >= 0);
  printf("search key: %d value: %.*s\n", i, rc, value);

  // 遍历操作
  db_check_all(db, callbakc, 1000);

  // 删除操作
  for (i = 0; i < COUNT; i++) {
    assert(db_delete(db, &i) == 1);
  }
  printf("delete key from %d to %d\n", 0, COUNT);

  // 关闭数据库
  db_close(db);

  return 0;
}
#endif

#if false
#include "SQLParser.h"
#include "SQLParserResult.h"
#include "sql/Expr.h"
#include <iostream>
int main(){
  hsql::SQLParserResult result;
  hsql::SQLParser::parse(
      "SELECT ssss,ggggg FROM test, yyyyy WHERE ssss = 1 AND ggggg = 2;",
      &result);
  if (!result.isValid()) {
    printf("Error: %s\n", result.errorMsg());
    return 0;
  }
  const hsql::SQLStatement *stmt = result.getStatement(0);
  if (stmt->type() == hsql::kStmtSelect) {
    const hsql::SelectStatement *sel =
        static_cast<const hsql::SelectStatement *>(stmt);
    auto g = sel->fromTable->list;
    for (auto i = g->begin(); i != g->end(); i++) {
      std::cout << "Table: " << (*i)->name << std::endl;
    }

    const auto &n = sel->selectList;
    for (auto i = n->begin(); i != n->end(); i++) {
      std::cout << "Column: " << (*i)->name << std::endl;
    }

    const auto& where = sel->whereClause;
    std::cout<<(where->opType == hsql::OperatorType::kOpAnd)<<std::endl;
  }
  return 0;
}

#endif

#if true
#include "database.h"
#include <filesystem>
#include <iostream>
#include <unistd.h>
const char *f = __FILE__;
int main() {
  std::filesystem::path filepath(f);
  auto dbPath = filepath.parent_path().parent_path() / "build" / "test.db";
  // unlink(dbPath.c_str());
  try {
    DataBase db(dbPath);
  } catch (std::exception &e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}

#endif