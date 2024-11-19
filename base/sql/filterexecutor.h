#pragma once
#include "executor.h"

class FilterExecutor : public Executor {
public:
  void next(const std::function<void(RowReader *reader)> &callback) override;
};