#include "filterexecutor.h"
#include "database.h"
#include "executor/seqexecutor.h"
#include <codecvt>
#include <locale>
FilterExecutor::FilterExecutor(DataBase *db, hsql::Expr *expr)
    : db(db), expr(expr) {
  if (expr->opType == hsql::kOpAnd || expr->opType == hsql::kOpOr) {
    auto left = new FilterExecutor(db, expr->expr);
    auto right = new FilterExecutor(db, expr->expr2);
    addChild(left);
    addChild(right);
    if (expr->opType == hsql::kOpAnd) {
      type = 0;
    } else {
      type = 2;
    }
  }

  if (expr->opType == hsql::kOpEquals) {
    auto colName = expr->expr->name;

    auto table1 = db->getTable(expr->expr->table);
    auto priIndex =
        std::find_if(table1->columns().begin(), table1->columns().end(),
                     [&](const Column &c) { return c.column_name == colName; });
    auto index = std::distance(table1->columns().begin(), priIndex);
    find_info.findIndex = index;
    if (!expr->expr2->hasTable()) {
      const auto &col = table1->columns()[index];
      find_info.DataType = col.data_type.type;
      if (expr->expr2->type == hsql::kExprLiteralInt) {
        find_info.findVal = &expr->expr2->ival;
      }
      if (expr->expr2->type == hsql::kExprLiteralString) {
        find_info.findVal = expr->expr2->name;
      }
      if (table1->columns()[table1->primaryKeyIndex()].column_name != colName) {
        auto exec = new SeqExecutor(db, table1->name());
        addChild(exec);
      } else {
        auto exec = new KeyExecutor(db, table1->name(), find_info.findVal);
        addChild(exec);
      }
      type = 1;
    }
  }
}

int FilterExecutor::compare(void *findVal, RowReader *r, int findIndex,
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

void FilterExecutor::next(
    const std::function<void(RowReader *reader)> &callback) {
  if (type == 0) {
    children[0]->next([&](RowReader *reader) {
      children[1]->next([&](RowReader *reader2) {
        // todo: dump
        if (reader->readInt64(0) == reader2->readInt64(0)) {
          callback(reader);
        }
      });
    });
    return;
  }
  if (type == 2) {
    children[0]->next(callback);
    children[1]->next(callback);
  }

  if (type == 1) {
    children[0]->next([&](RowReader *reader) {
      if (compare(find_info.findVal, reader, find_info.findIndex,
                  find_info.DataType) == 0) {
        callback(reader);
      }
    });
  }
}