#pragma once
#include "sql/SQLStatement.h"
#include <functional>

class DataBase;
class RowReader;
class SqlRunner {
public:
  /**
   * @brief execute the sql and return the error code
   *
   * @param sql sql string
   * @return int error code
   */
  virtual int execute(const hsql::SQLStatement *stm) = 0;
};

class SelectRunner : public SqlRunner {
private:
  DataBase *db;

  static int compare(void* findVal, RowReader* r, int findIndex, int DataType);
  void getEqual(void* findVal, const std::string& colName, const std::string& tableName, const std::function<void(RowReader* r)>& callBack);
public:
  SelectRunner(DataBase *db);
  int execute(const hsql::SQLStatement *stm) override;
};