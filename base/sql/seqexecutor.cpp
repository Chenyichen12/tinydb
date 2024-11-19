#include "seqexecutor.h"


void SeqExecutor::next(const std::function<void(RowReader *reader)> &callback) {
    db->getValue(find_table_, callback);
}