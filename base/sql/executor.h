#pragma once
#include <functional>

class RowReader;
class Executor {
protected:
  std::vector<Executor *> children;

public:
  virtual void next(const std::function<void(RowReader *reader)> &callback) = 0;
  void addChild(Executor *child);
  virtual ~Executor();
};

