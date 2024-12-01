#pragma once
#include "executor.h"
class OrderExecutor : public Executor {
  std::vector<int> orderedIndex;
  std::vector<int> orderedType; // 0 for i32 1 for i64 2 for float 3 for string

public:
  OrderExecutor(const std::vector<int> &orderedIndex,
                const std::vector<int> &orderedType);

  void next(const std::function<void(RowReader *reader)> &callback) override;

  template<typename  T>
  bool compare(const T &a, const T &b){
    return a < b;
  }
};