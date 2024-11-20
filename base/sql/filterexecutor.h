#pragma once
#include "executor.h"
#include "sql/Expr.h"
#include <optional>
#include <string>
class DataBase;
class FilterExecutor : public Executor {
  DataBase *db;
  hsql::Expr *expr;

  struct {
    void *findVal;
    int findIndex;
    int DataType;
  } find_info;

  int type = -1;

  int compare(void *findVal, RowReader *r, int findIndex, int DataType);

public:
  FilterExecutor(DataBase *db, hsql::Expr *expr);
  void next(const std::function<void(RowReader *reader)> &callback) override;
};