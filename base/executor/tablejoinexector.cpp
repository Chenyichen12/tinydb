#include "tablejoinexector.h"
#include "database.h"
#include "executor/seqexecutor.h"
TableJoinExecutor::TableJoinExecutor(DataBase *db, hsql::Expr *expr) : db(db) {
  tables.push_back(db->getTable(expr->expr->table));
  tables.push_back(db->getTable(expr->expr2->table));
}

// id == id situation
void TableJoinExecutor::next(
    const std::function<void(RowReader *reader)> &callback) {

  auto firstSeq = std::make_unique<SeqExecutor>(db, tables[0]->name());
  firstSeq->next([&](RowReader *reader1) {
    auto primaryKey = reader1->readInt64(0);
    auto secondSeq =
        std::make_unique<KeyExecutor>(db, tables[1]->name(), &primaryKey);
    secondSeq->next([&](RowReader *reader2) {
      // merge two row
      std::vector<Column> colums;
      for (auto &col : tables[0]->columns()) {
        colums.push_back(col);
      }
      for (auto &col : tables[1]->columns()) {
        colums.push_back(col);
      }

      auto dataBuf = new char[reader1->byteSize() + reader2->byteSize()];

      reader1->readByte(dataBuf, reader1->byteSize(), 0);
      reader2->readByte(dataBuf, reader2->byteSize(), reader1->byteSize());
      auto newReader = new RowReader(colums, dataBuf);
      callback(newReader);

      delete[] dataBuf;
    });
  });
}
