#pragma once
#include "executor.h"
#include <sql/Expr.h>
#include "table.h"
#include <string>
class DataBase;
class TableJoinExecutor : public Executor {
  DataBase *db;
  std::vector<const Table *> tables;

public:
  TableJoinExecutor(DataBase *db, hsql::Expr *expr);
  void next(const std::function<void(RowReader *reader)> &callback) override;
};