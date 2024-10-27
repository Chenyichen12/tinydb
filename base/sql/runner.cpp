#include "runner.h"
#include "database.h"
#include "sql/SelectStatement.h"
#include <algorithm>
#include <codecvt>
#include <iostream>
#include <locale>
SelectRunner::SelectRunner(DataBase *db) : db(db) {}

int SelectRunner::execute(const hsql::SQLStatement *stm) {
  auto sel = static_cast<const hsql::SelectStatement *>(stm);

  auto where = sel->whereClause;
  std::vector<std::string> fromTable;
  if (sel->fromTable->list == nullptr) {
    fromTable.push_back(sel->fromTable->name);
  } else {
    for (const auto &table : *(sel->fromTable->list)) {
      fromTable.push_back(table->name);
    }
  }

  // if table has two or more
  for (const auto &table : fromTable) {
    // test is exist
    if (!db->tableExist(table)) {
      std::cout << "table " << table << " is not exist" << std::endl;
      return 1;
    }
  }

  auto where_colum = where->expr->table;
  auto where_table = where->expr->name;
  auto find = where->expr2->name;

  getEqual(find, where_table, where_colum, [&](RowReader *r) {
    auto id = r->readInt64(0);
    auto name = r->readString(1);

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::string narrow_string = converter.to_bytes(name);

    auto age = r->readInt32(2);
    std::cout << "id: " << id << " name: " << narrow_string << " age: " << age
              << std::endl;
  });
  std::cout << "select done" << std::endl;
  return 0;
}

int SelectRunner::compare(void *findVal, RowReader *r, int findIndex,
                          int DataType) {

  switch (DataType) {
  case DataType::Type::INT32: {
    auto val = r->readInt32(findIndex);
    auto testVal = *(int32_t *)findVal;
    if (val == testVal) {
      return 0;
    }
    if (val > testVal) {
      return 1;
    }
    if (val < testVal) {
      return 2;
    }
    break;
  }
  case DataType::Type::INT64: {
    auto val = r->readInt64(findIndex);
    auto testVal = *(int64_t *)findVal;
    if (val == testVal) {
      return 0;
    }
    if (val > testVal) {
      return 1;
    }
    if (val < testVal) {
      return 2;
    }
    break;
  }
  case DataType::Type::FLOAT: {
    auto val = r->readFloat(findIndex);
    auto testVal = *(float *)findVal;
    if (val == testVal) {
      return 0;
    }
    if (val > testVal) {
      return 1;
    }
    if (val < testVal) {
      return 2;
    }
    break;
  }
  case DataType::Type::STRING: {
    auto val = r->readString(findIndex);
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::string narrow_string = converter.to_bytes(val);

    const auto &testVal = std::string((char *)findVal);
    if (narrow_string == testVal) {
      return 0;
    }
    if (narrow_string > testVal) {
      return 1;
    }
    if (narrow_string < testVal) {
      return 2;
    }
    break;
  }
  case DataType::Type::BOOL: {
    auto val = r->readBool(findIndex);
    auto testVal = *(bool *)findVal;
    if (val == testVal) {
      return 0;
    }
    if (val > testVal) {
      return 1;
    }
    if (val < testVal) {
      return 2;
    }
    break;
  }
  default:
    break;
  }
  return -1;
}

void SelectRunner::getEqual(void *findVal, const std::string &colName,
                            const std::string &tableName,
                            const std::function<void(RowReader *r)> &callBack) {
  auto table = db->getTable(tableName);
  auto pri = table->columns()[table->primaryKeyIndex()].column_name;
  if (pri == colName) {
    auto res = db->getValue(tableName, findVal,
                            [&](RowReader *reader) { callBack(reader); });
  } else {
    auto colums = table->columns();
    auto priIndex =
        std::find_if(colums.begin(), colums.end(),
                     [&](const Column &c) { return c.column_name == colName; });
    int index = std::distance(colums.begin(), priIndex);
    auto type = priIndex->data_type.type;
    db->getValue(tableName, [&](RowReader *reader) {
      auto result = compare(findVal, reader, index, type);
      if (result == 0) {
        callBack(reader);
      }
    });
  }
}
