#include "runner.h"
#include "seqexecutor.h"
#include "sql/CreateStatement.h"
#include "sql/DeleteStatement.h"
#include "sql/InsertStatement.h"
#include "sql/SelectStatement.h"
#include "sql/filterexecutor.h"
#include <algorithm>
#include <codecvt>
#include <cstring>
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
      auto seqexecutor = SeqExecutor(db, table->name());
      seqexecutor.next([&](RowReader *reader) { output.output(reader); });
      return 0;
    }

    auto where = sel->whereClause;
    auto filterexecutor = new FilterExecutor(db, where);
    filterexecutor->next([&](RowReader *reader) { output.output(reader); });
  }
  std::cout << "-----------------" << std::endl;
  std::cout << "Select Done" << std::endl;
  return 0;
}

InsertRunner::InsertRunner(DataBase *db) : db(db) {}

int InsertRunner::execute(const hsql::SQLStatement *stm) {
  auto sel = static_cast<const hsql::InsertStatement *>(stm);
  auto tableName = sel->tableName;
  if (!db->tableExist(tableName)) {
    std::cout << "table " << tableName << " is not exist" << std::endl;
    return 1;
  }

  auto table = db->getTable(tableName);
  for (const auto &col : *sel->columns) {
    // std::cout << col << '\n';
    if (!table->hasColumn(col)) {
      std::cout << "column " << col << " is not exist" << std::endl;
      return 2;
    }
  }

  auto result = db->insertValue(tableName, [&](RowBuilder *rowBuilder) {
    for (const auto &col : *sel->columns) {

      auto index = table->getColumnIndex(col);
      auto value = sel->values->at(index);

      if (value->isType(hsql::kExprLiteralString)) {
        std::string str = value->getName();
        rowBuilder->addValue(str);
      }
      if (value->isType(hsql::kExprLiteralInt) ||
          value->isType(hsql::kExprLiteralFloat)) {

        auto type = table->valueType(col);
        if (type.type == DataType::Type::INT32) {
          rowBuilder->addValue((int32_t)value->ival);
        }
        if (type.type == DataType::Type::INT64) {
          rowBuilder->addValue((int64_t)value->ival);
        }
        if (type.type == DataType::Type::FLOAT) {
          rowBuilder->addValue((float)value->fval);
        }
        if (type.type == DataType::Type::BOOL) {
          rowBuilder->addValue((bool)value->ival);
        }
      }
    }
  });
  if (result != 0) {
    std::cout << "insert failed" << std::endl;
    return 1;
  } else {
    std::cout << "insert success" << std::endl;
  }

  return 0;
}

DeleteRunner::DeleteRunner(DataBase *db) : db(db) {}

int DeleteRunner::execute(const hsql::SQLStatement *stm) {
  auto sel = static_cast<const hsql::DeleteStatement *>(stm);
  auto tableName = sel->tableName;
  if (!db->tableExist(tableName)) {
    std::cout << "table " << tableName << " is not exist" << std::endl;
    return 1;
  }

  auto table = db->getTable(tableName);
  if (sel->expr == nullptr) {
    std::cout << "not support delete all" << std::endl;
    return 1;
  }

  auto where = sel->expr;
  auto filterexecutor = new FilterExecutor(db, where);

  auto index = table->primaryKeyIndex();
  const auto &col = table->columns()[table->primaryKeyIndex()];
  auto type = col.data_type.type;
  std::vector<std::unique_ptr<char *>> prepareDeleteItem;
  filterexecutor->next([&](RowReader *reader) {
    auto val = std::make_unique<char *>(nullptr);

    switch (type) {
    case DataType::INT32: {
      val = std::make_unique<char *>(new char[4]);
      auto v = reader->readInt32(index);
      memcpy(*val, &v, 4);
      break;
    }
    case DataType::INT64: {
      val = std::make_unique<char *>(new char[8]);
      auto v = reader->readInt64(index);
      memcpy(*val, &v, 8);
      break;
    }
    case DataType::STRING: {
      auto v = reader->readString(index);
      val = std::make_unique<char *>(new char[v.size()]);
      memcpy(*val, v.c_str(), v.size());
      break;
    }
    default:
      break;
    }
    if (val != nullptr) {
      prepareDeleteItem.push_back(std::move(val));
    }
  });

  bool isSuccess = true;
  for (auto &val : prepareDeleteItem) {
    auto result = table->deleteValue(*val);
    if (result == 0) {
      std::cout << "can't find the value to delete" << std::endl;
      isSuccess = false;
    }
    if (result == -1) {
      std::cout << "delete failed" << std::endl;
      isSuccess = false;
      break;
    }
  }
  if (isSuccess) {
    std::cout << "delete done" << std::endl;
  }
  return 0;
}

CreateTableRunner::CreateTableRunner(DataBase *db) : db(db) {}

int CreateTableRunner::execute(const hsql::SQLStatement *stm) {
  auto sel = static_cast<const hsql::CreateStatement *>(stm);
  auto tableName = sel->tableName;

  if (db->tableExist(tableName)) {
    std::cout << "table " << tableName << " is exist" << std::endl;
    return 1;
  }

  auto firstCol = sel->columns->at(0);
  if (firstCol->type.data_type != hsql::DataType::INT &&
      firstCol->type.data_type != hsql::DataType::LONG) {
    std::cout << "first column must be int" << std::endl;
    return 1;
  }

  auto result = db->addTable([&](TableBuilder *builder) {
    builder->setName(tableName);
    for (const auto &col : *sel->columns) {
      DataType type = DataType::Int32();
      switch (col->type.data_type) {
      case hsql::DataType::INT: {
        type = DataType::Int32();
        break;
      }
      case hsql::DataType::LONG: {
        type = DataType::Int64();
        break;
      }
      case hsql::DataType::FLOAT: {
        type = DataType::Float();
        break;
      }
      case hsql::DataType::VARCHAR: {
        auto length = col->type.length;
        type = DataType::String(length);
        break;
      }
      default:
        break;
      }
      builder->addColumn(col->name, type);
    }
    builder->setPrimaryKey(0);
  });

  if(result != 0){
    std::cout << "create table failed" << std::endl;
    return 1;
  }

  std::cout << "create table success" << std::endl;

  return 0;
}