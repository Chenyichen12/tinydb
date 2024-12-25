#pragma once
#include "database.h"
#include "sql/SQLStatement.h"
#include <functional>

class SqlOutput {

public:
  struct ColDefinition {
    std::string name;
    size_t rowIndex;
    DataType::Type type;
  };
  SqlOutput(const std::vector<ColDefinition> &outputColumns);

  void outTitle() const;
  void output(RowReader *r) const;

private:
  std::vector<ColDefinition> output_cols_;
};

class SqlRunner {
public:
  /**
   * @brief execute the sql and return the error code
   *
   * @param sql sql string
   * @return int error code
   */
  virtual int execute(const hsql::SQLStatement *stm) = 0;
  virtual ~SqlRunner() = default;
};

class SelectRunner : public SqlRunner {
private:
  DataBase *db;

  static int compare(void *findVal, RowReader *r, int findIndex, int DataType);

  void getEqual(void *findVal, const std::string &colName,
                const std::string &tableName,
                const std::function<void(RowReader *r)> &callBack);

public:
  SelectRunner(DataBase *db);
  int execute(const hsql::SQLStatement *stm) override;
};

class InsertRunner : public SqlRunner {
  DataBase *db;

public:
  InsertRunner(DataBase *db);
  int execute(const hsql::SQLStatement *stm) override;
};

class DeleteRunner : public SqlRunner {
  DataBase *db;

public:
  DeleteRunner(DataBase *db);
  int execute(const hsql::SQLStatement *stm) override;
};

class CreateTableRunner : public SqlRunner {
  DataBase *db;

public:
  CreateTableRunner(DataBase *db);
  int execute(const hsql::SQLStatement *stm) override;
};

class UpdateRunner : public SqlRunner {
  DataBase *db;

  static std::pair<int, int>
  memberOffset(const std::vector<Column> &columnsDefination,
         const std::string &colName);

  static void updateBuf(char* buf, int offset, int length, const char* val, int valLength);

public:
  UpdateRunner(DataBase *db);
  int execute(const hsql::SQLStatement *stm) override;
};