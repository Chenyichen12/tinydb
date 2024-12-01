#include "limitexecutor.h"

void LimitExecutor::next(
    const std::function<void(RowReader *reader)> &callback) {
  auto child = children[0];
  int count = 0;
  child->next([&](RowReader *reader) {
    if (count >= offset_ && count < limit_ + offset_) {
      callback(reader);
    }
    count++;
  });
}