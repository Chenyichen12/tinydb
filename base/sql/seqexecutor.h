#pragma once
#include "../database.h"
#include "executor.h"
class SeqExecutor : public Executor {
private:
  DataBase *db;
  std::string find_table_;

public:
  SeqExecutor(DataBase *db, const std::string &find_table)
      : db(db), find_table_(find_table) {}
  void next(const std::function<void(RowReader *reader)> &callback) override;
};