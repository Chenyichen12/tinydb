#include "executor.h"

void Executor::addChild(Executor *child) { children.push_back(child); }

Executor::~Executor() {
  for (Executor *child : children) {
    delete child;
  }
}
