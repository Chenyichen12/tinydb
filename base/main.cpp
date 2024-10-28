
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

#if false
#include "database.h"
#include <cassert>
#include <codecvt>
#include <filesystem>
#include <iostream>
#include <unistd.h>
#define READ true
const char *f = __FILE__;
int main() {
  std::filesystem::path filepath(f);
  auto dbPath = filepath.parent_path().parent_path() / "build" / "test.db";
  auto testDbPath =
      filepath.parent_path().parent_path() / "build" / "student.db";
#if READ
  unlink(dbPath.c_str());
  unlink(testDbPath.c_str());

  try {
    DataBase db(dbPath);
    auto res = db.addTable([](TableBuilder *b) {
      b->setName("student");
      b->addColumn("id", DataType::Int64());
      b->addColumn("name", DataType::String(128));
      b->addColumn("age", DataType::Int32());
      b->addColumn("sex", DataType::Bool());
      b->setPrimaryKey("id");
    });
    assert(res == 0);

    db.saveConfig();

    res = db.insertValue("student", [](RowBuilder *b) {
      try {
        b->addValue((int64_t)(1111));
        b->addValue(std::wstring(L"伊见"));
        b->addValue(16);
        b->addValue(false);
      } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
      }
    });

    res = db.insertValue("student", [](RowBuilder *b) {
      try {
        b->addValue((int64_t)(2222));
        b->addValue(std::wstring(L"怡雏"));
        b->addValue(17);
        b->addValue(false);
      } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
      }
    });

    res = db.insertValue("student", [](RowBuilder *b) {
      try {
        b->addValue((int64_t)(3333));
        b->addValue(std::wstring(L"逸佳"));
        b->addValue(18);
        b->addValue(true);
      } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
      }
    });

    res = db.insertValue("student", [](RowBuilder *b) {
      try {
        b->addValue((int64_t)(4444));
        b->addValue(std::wstring(L"依澄"));
        b->addValue(18);
        b->addValue(true);
      } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
      }
    });

    assert(res == 0);

    res = db.getValue("student", [](RowReader *reader) {
      auto id = reader->readInt64(0);
      auto name = reader->readString(1);

      std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
      std::string narrow_string = converter.to_bytes(name);

      auto age = reader->readInt32(2);
      bool sex = reader->readBool(3);
      std::cout << "id: " << id << " name:" << narrow_string << " age: " << age
                << " sex:" << sex << std::endl;
    });
    assert(res == 0);
    
    auto index = std::make_unique<int64_t>(4444);
    res = db.getValue("student", index.get(), [](RowReader *reader) {
      std::cout << "id 4444 has found" << std::endl;
      auto id = reader->readInt64(0);
      auto name = reader->readString(1);

      std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
      std::string narrow_string = converter.to_bytes(name);

      auto age = reader->readInt32(2);
      bool sex = reader->readBool(3);
      std::cout << "id: " << id << " name:" << narrow_string << " age: " << age
                << " sex:" << sex << std::endl;
    });
    assert(res == 0);
  } catch (std::exception &e) {
    std::cout << e.what() << std::endl;
  }
#else
  try {
    DataBase db(dbPath);
  } catch (std::exception &e) {
    std::cout << e.what() << std::endl;
  }
#endif

  return 0;
}

#endif

#if true
#include "database.h"
#include "sql/runner.h"
#include <SQLParser.h>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <unistd.h>
constexpr const char *f = __FILE__;
int main() {
  std::filesystem::path filepath(f);
  auto dbPath = filepath.parent_path().parent_path() / "build" / "test.db";
  auto testDbPath =
      filepath.parent_path().parent_path() / "build" / "student.db";

  unlink(dbPath.c_str());
  unlink(testDbPath.c_str());

  DataBase db(dbPath);
  auto res = db.addTable([](TableBuilder *b) {
    b->setName("student");
    b->addColumn("id", DataType::Int64());
    b->addColumn("name", DataType::String(128));
    b->addColumn("age", DataType::Int32());
    b->addColumn("sex", DataType::Bool());
    b->setPrimaryKey("id");
  });
  assert(res == 0);
  db.saveConfig();

  res = db.insertValue("student", [](RowBuilder *b) {
    try {
      b->addValue((int64_t)(1111));
      b->addValue(std::wstring(L"伊见"));
      b->addValue(16);
      b->addValue(false);
    } catch (std::exception &e) {
      std::cout << e.what() << std::endl;
    }
  });

  res = db.insertValue("student", [](RowBuilder *b) {
    try {
      b->addValue((int64_t)(2222));
      b->addValue(std::wstring(L"怡雏"));
      b->addValue(17);
      b->addValue(false);
    } catch (std::exception &e) {
      std::cout << e.what() << std::endl;
    }
  });

  res = db.insertValue("student", [](RowBuilder *b) {
    try {
      b->addValue((int64_t)(3333));
      b->addValue(std::wstring(L"逸佳"));
      b->addValue(18);
      b->addValue(true);
    } catch (std::exception &e) {
      std::cout << e.what() << std::endl;
    }
  });

  res = db.insertValue("student", [](RowBuilder *b) {
    try {
      b->addValue((int64_t)(4444));
      b->addValue(std::wstring(L"依澄"));
      b->addValue(18);
      b->addValue(true);
    } catch (std::exception &e) {
      std::cout << e.what() << std::endl;
    }
  });
  assert(res == 0);

  // const char *testSelectSql =
  //     "SELECT student.name, student.age FROM student WHERE student.name =
  //     '依澄';";
  // const char* testSelectSql2 = "SELECT student.name, student.age FROM
  // student;";
  std::string input;
  std::getline(std::cin, input);

  hsql::SQLParserResult result;
  hsql::SQLParser::parse(input.c_str(), &result);
  if (!result.isValid()) {
    printf("Error: %s\n", result.errorMsg());
    return 0;
  }
  const hsql::SQLStatement *stmt = result.getStatement(0);
  SelectRunner runner(&db);
  runner.execute(stmt);

  return 0;
}
#endif