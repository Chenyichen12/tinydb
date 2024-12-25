#include "runner.h"
#include "executor/filterexecutor.h"
#include "executor/limitexecutor.h"
#include "executor/orderexecutor.h"
#include "executor/seqexecutor.h"
#include "executor/tablejoinexector.h"
#include "sql/CreateStatement.h"
#include "sql/DeleteStatement.h"
#include "sql/InsertStatement.h"
#include "sql/SelectStatement.h"
#include "sql/UpdateStatement.h"
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

    auto firstTable = fromTable[0];
    auto secondTable = fromTable[1];

    if (!db->tableExist(firstTable)) {
      std::cout << "table " << firstTable << " is not exist" << std::endl;
      return 1;
    }

    if (!db->tableExist(secondTable)) {
      std::cout << "table " << secondTable << " is not exist" << std::endl;
      return 1;
    }

    auto table1 = db->getTable(firstTable);
    auto table2 = db->getTable(secondTable);

    std::vector<SqlOutput::ColDefinition> outputCols;
    auto list = *sel->selectList;
    if (list[0]->type == hsql::kExprStar) {
      for (size_t i = 0; i < table1->columns().size(); i++) {
        outputCols.push_back({table1->columns()[i].column_name, i,
                              table1->columns()[i].data_type.type});
      }
      int offset = table1->columns().size();
      for (size_t i = 0; i < table2->columns().size(); i++) {
        outputCols.push_back({table2->columns()[i].column_name, i + offset,
                              table2->columns()[i].data_type.type});
      }
    } else {
      for (const auto &s : list) {
        if (s->table == firstTable) {
          auto colIndex = std::find_if(
              table1->columns().begin(), table1->columns().end(),
              [&](const Column &c) { return c.column_name == s->name; });
          if (colIndex == table1->columns().end()) {
            std::cout << "column " << s->name << " is not exist" << std::endl;
            return 1;
          }
          size_t index = std::distance(table1->columns().begin(), colIndex);
          outputCols.push_back({s->name, index, colIndex->data_type.type});
        } else {
          int offset = table1->columns().size();
          auto colIndex = std::find_if(
              table2->columns().begin(), table2->columns().end(),
              [&](const Column &c) { return c.column_name == s->name; });
          if (colIndex == table2->columns().end()) {
            std::cout << "column " << s->name << " is not exist" << std::endl;
            return 1;
          }
          size_t index = std::distance(table2->columns().begin(), colIndex);
          outputCols.push_back(
              {s->name, index + offset, colIndex->data_type.type});
        }
      }
    }

    // test
    // for (const auto &col : outputCols) {
    //   std::cout << col.name << " " << col.rowIndex << " " << col.type
    //             << std::endl;
    // }

    SqlOutput output(outputCols);
    output.outTitle();

    auto join = new TableJoinExecutor(db, sel->whereClause);
    join->next([&](RowReader *reader) { output.output(reader); });
    return 0;
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

    std::vector<Executor *> executors;

    if (sel->limit != nullptr) {
      auto limit = sel->limit->limit->ival;
      int offset = 0;
      if (sel->limit->offset != nullptr) {
        offset = sel->limit->offset->ival;
      }
      auto limitexecutor = new LimitExecutor(limit, offset);
      executors.push_back(limitexecutor);
    }
    if (sel->order != nullptr) {
      std::vector<int> colIndex;
      std::vector<int> colType;
      for (const auto &o : *sel->order) {
        if (!db->tableExist(o->expr->table)) {
          std::cout << "table " << table << " is not exist" << std::endl;
          return 1;
        }

        auto col = table->getColumnIndex(o->expr->name);
        if (col == -1) {
          std::cout << "column " << o->expr->name << " is not exist"
                    << std::endl;
          return 1;
        }
        auto rCol = table->columns()[col];
        switch (rCol.data_type.type) {
        case DataType::INT32:
          colType.push_back(0);
          break;
        case DataType::INT64:
          colType.push_back(1);
          break;
        case DataType::FLOAT:
          colType.push_back(2);
          break;
        case DataType::STRING:
          colType.push_back(3);
          break;
        default:
          colType.push_back(-1);
          break;
        }
        colIndex.push_back(col);
      }
      auto orderexecutor = new OrderExecutor(colIndex, colType);
      if (executors.empty()) {
        executors.push_back(orderexecutor);
      } else {
        executors.back()->addChild(orderexecutor);
        executors.push_back(orderexecutor);
      }
    }

    if (sel->whereClause == nullptr) {
      auto seqexecutor = new SeqExecutor(db, table->name());
      if (executors.empty()) {
        executors.push_back(seqexecutor);
      } else {
        executors.back()->addChild(seqexecutor);
        executors.push_back(seqexecutor);
      }
    } else {
      auto where = sel->whereClause;
      auto filterexecutor = new FilterExecutor(db, where);
      if (executors.empty()) {
        executors.push_back(filterexecutor);
      } else {
        executors.back()->addChild(filterexecutor);
        executors.push_back(filterexecutor);
      }
    }
    executors[0]->next([&](RowReader *reader) { output.output(reader); });

    delete executors[0];
    return 0;
  }

  std::cout << "-----------------" << std::endl;
  std::cout << "Select Done" << std::endl;
  return 0;
}

InsertRunner::InsertRunner(DataBase *db) : db(db) {}

int InsertRunner::execute(const hsql::SQLStatement *stm) {
  auto sel = static_cast<const hsql::InsertStatement *>(stm);
  auto tableName = sel->tableName; // 得到表名
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
  std::vector<std::unique_ptr<char[]>> prepareDeleteItem;
  filterexecutor->next([&](RowReader *reader) {
    std::unique_ptr<char[]> val;

    switch (type) {
    case DataType::INT32: {
      val = std::make_unique<char[]>(4);
      auto v = reader->readInt32(index);
      memcpy(val.get(), &v, 4);
      break;
    }
    case DataType::INT64: {
      val = std::make_unique<char[]>(8);
      auto v = reader->readInt64(index);
      memcpy(val.get(), &v, 8);
      break;
    }
    case DataType::STRING: {
      auto v = reader->readString(index);
      val = std::make_unique<char[]>(v.size());
      memcpy(val.get(), v.c_str(), v.size());
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
    auto result = table->deleteValue(val.get());
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

  if (result != 0) {
    std::cout << "create table failed" << std::endl;
    return 1;
  }

  db->saveConfig();
  std::cout << "create table success" << std::endl;

  return 0;
}

UpdateRunner::UpdateRunner(DataBase *db) : db(db) {}

std::pair<int, int>
UpdateRunner::memberOffset(const std::vector<Column> &columnsDefination,
                           const std::string &colName) {
  int offset = 0;
  int length = 0;
  for (int i = 0; i < columnsDefination.size(); i++) {
    if (columnsDefination[i].column_name == colName) {
      length = columnsDefination[i].data_type.size;
      break;
    }
    offset += columnsDefination[i].data_type.size;
  }

  return {offset, length};
}

void UpdateRunner::updateBuf(char *buf, int offset, int length, const char *val,
                             int valLength) {
  memcpy(buf + offset, val, valLength);
}
int UpdateRunner::execute(const hsql::SQLStatement *stm) {
  const auto sel = static_cast<const hsql::UpdateStatement *>(stm);
  const auto tableRef = sel->table;

  if (!db->tableExist(tableRef->name)) {
    std::cout << "table " << tableRef->name << " is not exist" << std::endl;
    return 1;
  }

  auto table = db->getTable(tableRef->name);
  // check column
  for (const auto &update : *sel->updates) {
    // column
    if (!table->hasColumn(update->column)) {
      std::cout << "column " << update->column << " is not exist" << std::endl;
      return 1;
    }
  }
  std::unique_ptr<Executor> exec;
  try {
    if (sel->where == nullptr) {
      exec = std::make_unique<SeqExecutor>(db, table->name());
    } else {
      exec = std::make_unique<FilterExecutor>(db, sel->where);
    }
  } catch (...) {
    std::cout << "where clause error" << std::endl;
    return 1;
  }

  std::vector<std::unique_ptr<char[]>> prepareChangedValue;

  exec->next([&](RowReader *reader) {
    auto buf = std::make_unique<char[]>(reader->byteSize());
    reader->readByte(buf.get(), reader->byteSize());
    prepareChangedValue.push_back(std::move(buf));
  });

  for (int i = 0; i < prepareChangedValue.size(); i++) {
    for (const auto &update : *sel->updates) {
      auto info = memberOffset(table->columns(), update->column);
      auto changeVal = std::make_unique<char[]>(info.second);
      switch (update->value->type) {
      case hsql::kExprLiteralString: {
        auto str = update->value->getName();
        auto converter = std::wstring_convert<std::codecvt_utf8<wchar_t>>();
        auto wstr = converter.from_bytes(str);
        memcpy(changeVal.get(), wstr.c_str(), info.second);
        break;
      }
      case hsql::kExprLiteralInt: {
        auto val = update->value->ival;
        memcpy(changeVal.get(), &val, info.second);
        break;
      }
      default: {
        std::cout << "not support" << std::endl;
        return 1;
      }
      }

      updateBuf(prepareChangedValue[i].get(), info.first, info.second,
                changeVal.get(), info.second);
    }
  }

  // test change

  // auto reader = new RowReader(table->columns(),
  // prepareChangedValue[0].get()); std::vector<SqlOutput::ColDefinition>
  // outputCols; for (size_t i = 0; i < table->columns().size(); i++) {
  //   const auto &col = table->columns()[i];
  //   outputCols.push_back({col.column_name, i, col.data_type.type});
  // }
  // SqlOutput output(outputCols);
  // output.outTitle();
  // output.output(reader);

  // delete old
  auto pKeyIndex = table->primaryKeyIndex();
  auto pKeyName = table->columns()[pKeyIndex].column_name;
  auto keyInfo = memberOffset(table->columns(), pKeyName);
  for (int i = 0; i < prepareChangedValue.size(); i++) {
    auto pKeyVal = std::unique_ptr<char[]>(new char[keyInfo.second]);
    memcpy(pKeyVal.get(), prepareChangedValue[i].get() + keyInfo.first,
           keyInfo.second);
    auto result = table->deleteValue(pKeyVal.get());
    if (result == 0) {
      std::cout << "can't find the value to delete" << std::endl;
    }
    if (result == -1) {
      std::cout << "delete failed" << std::endl;
      return 1;
    }
  }

  // insert new
  for (int i = 0; i < prepareChangedValue.size(); i++) {
    auto pKeyVal = std::unique_ptr<char[]>(new char[keyInfo.second]);
    memcpy(pKeyVal.get(), prepareChangedValue[i].get() + keyInfo.first,
           keyInfo.second);
    auto result = db->insertValue(table->name(), pKeyVal.get(),
                                  prepareChangedValue[i].get());
    if (result != 0) {
      std::cout << "update failed" << std::endl;
      return 1;
    }
  }
  std::cout << "update done" << std::endl;
  std::cout << "update " << prepareChangedValue.size() << " records"
            << std::endl;
  return 0;
}
