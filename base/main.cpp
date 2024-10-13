
#include "SQLParser.h"
#include "SQLParserResult.h"
#include "sql/Expr.h"
#include <iostream>
int main() {
  hsql::SQLParserResult result;
  hsql::SQLParser::parse(
      "SELECT ssss,ggggg FROM test, yyyyy WHERE ssss = 1 AND ggggg = 2;",
      &result);
  if (!result.isValid()) {
    printf("Error: %s\n", result.errorMsg());
    return 0;
  }
  const hsql::SQLStatement *stmt = result.getStatement(0);
  if (stmt->type() == hsql::kStmtSelect) {
    const hsql::SelectStatement *sel =
        static_cast<const hsql::SelectStatement *>(stmt);
    auto g = sel->fromTable->list;
    for (auto i = g->begin(); i != g->end(); i++) {
      std::cout << "Table: " << (*i)->name << std::endl;
    }

    const auto &n = sel->selectList;
    for (auto i = n->begin(); i != n->end(); i++) {
      std::cout << "Column: " << (*i)->name << std::endl;
    }

    const auto& where = sel->whereClause;
    std::cout<<(where->opType == hsql::OperatorType::kOpAnd)<<std::endl;
  }
  return 0;
}