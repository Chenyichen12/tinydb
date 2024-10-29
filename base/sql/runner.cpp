#include "runner.h"
#include "sql/SelectStatement.h"
#include <algorithm>
#include <codecvt>
#include <iostream>
#include <locale>
SqlOutput::SqlOutput(const std::vector<ColDefinition> &outputColumns)
    : output_cols_(outputColumns) {}
void SqlOutput::outTitle() const {
  for (const auto &col : output_cols_) {
    std::cout << col.name << "\t";
  }
  std::cout << std::endl;
  for (const auto &col : output_cols_) {
    std::cout << "-------";
  }
  std::cout << std::endl;
}
void SqlOutput::output(RowReader *r) const {
  for (const auto &col : output_cols_) {
    switch (col.type) {
    case DataType::Type::INT32: {
      auto val = r->readInt32(col.rowIndex);
      std::cout << val << "\t";
      break;
    }
    case DataType::INT64: {
      auto val = r->readInt64(col.rowIndex);
      std::cout << val << "\t";
      break;
    }
    case DataType::FLOAT: {
      auto val = r->readFloat(col.rowIndex);
      std::cout << val << "\t";
      break;
    }
    case DataType::STRING: {
      auto val = r->readString(col.rowIndex);
      std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
      std::string narrow_string = converter.to_bytes(val);
      std::cout << narrow_string << "\t";
      break;
    }
    case DataType::BOOL: {
      auto val = r->readBool(col.rowIndex);
      std::cout << val << "\t";
      break;
    }
    default:
      break;
    }
  }
  std::cout << std::endl;
}
SelectRunner::SelectRunner(DataBase *db) : db(db) {}

int SelectRunner::execute(const hsql::SQLStatement *stm) {
  auto sel = static_cast<const hsql::SelectStatement *>(stm);

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

  // just one table or two table
  if (fromTable.size() > 1) {
    std::cout << "not support three or more table" << std::endl;
    return 1;
  }

  if (fromTable.size() == 1) {
    const auto &table = db->getTable(fromTable[0]);
    std::vector<SqlOutput::ColDefinition> outputCols;
    auto list = *sel->selectList;
    if (list[0]->type == hsql::kExprStar) {
      for (size_t i = 0; i < table->columns().size(); i++) {
        outputCols.push_back({table->columns()[i].column_name, i,
                              table->columns()[i].data_type.type});
      }
    } else {
      for (const auto &s : list) {
        auto colIndex = std::find_if(
            table->columns().begin(), table->columns().end(),
            [&](const Column &c) { return c.column_name == s->name; });
        if (colIndex == table->columns().end()) {
          std::cout << "column " << s->name << " is not exist" << std::endl;
          return 1;
        }
        size_t index = std::distance(table->columns().begin(), colIndex);
        outputCols.push_back({s->name, index, colIndex->data_type.type});
      }
    }

    SqlOutput output(outputCols);
    output.outTitle();

    if (sel->whereClause == nullptr) {
      db->getValue(table->name(),
                   [&](RowReader *reader) { output.output(reader); });
      return 0;
    }

    auto where = sel->whereClause;
    auto isPrimary = [&]() {
      if (where->opType == hsql::kOpEquals) {
        if (where->expr->type == hsql::kExprColumnRef &&
            where->expr2->type != hsql::kExprColumnRef) {
          auto colName = where->expr->name;
          if (table->columns()[table->primaryKeyIndex()].column_name ==
              colName) {
            return true;
          }
        }
      }
      return false;
    };

    if (isPrimary()) {
      auto colName = where->expr->name;
      void *findVal = nullptr;
      if (where->expr2->type == hsql::kExprLiteralInt) {
        findVal = &where->expr2->ival;
      }
      if (where->expr2->type == hsql::kExprLiteralString) {
        findVal = where->expr2->name;
      }

      getEqual(findVal, colName, table->name(),
               [&](RowReader *reader) { output.output(reader); });
    }
  }
  std::cout << "-----------------" << std::endl;
  std::cout << "Select Done" << std::endl;
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
