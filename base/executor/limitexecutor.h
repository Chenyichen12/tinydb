#pragma once
#include "executor.h"
class LimitExecutor : public Executor {
  int limit_;
  int offset_ = 0;

public:
  LimitExecutor(int limit, int offset) : limit_(limit), offset_(offset) {};
  void next(const std::function<void(RowReader *reader)> &callback) override;
};