#include "orderexecutor.h"
#include "database.h"
OrderExecutor::OrderExecutor(const std::vector<int> &orderedIndex,
                             const std::vector<int> &orderedType)
    : orderedIndex(orderedIndex), orderedType(orderedType) {}

void OrderExecutor::next(
    const std::function<void(RowReader *reader)> &callback) {
  std::vector<RowReader *> datas;
  auto child = children[0];

  child->next([&](RowReader *r) {
    auto newReader = r->clone();
    datas.push_back(newReader);
  });

  std::sort(datas.begin(), datas.end(), [&](RowReader *a, RowReader *b) {
    for (int i = 0; i < orderedIndex.size(); i++) {
      auto index = orderedIndex[i];
      auto type = orderedType[i];
      switch (type) {
      case 0:
        return a->readInt32(index) < b->readInt32(index);
      case 1:
        return a->readInt64(index) < b->readInt64(index);
      case 2:
        return a->readFloat(index) < b->readFloat(index);
      case 3:
        return a->readString(index) < b->readString(index);
      }
    }
    return false;
  });

  for (auto data : datas) {
    callback(data);
  }

  for (auto data : datas) {
    delete data;
  }
}