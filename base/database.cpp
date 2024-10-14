#include "database.h"
#include "file/store.h"
#include <fcntl.h> // for open(), O_CREAT, O_RDWR, O_TRUNC
#include <iostream>
#include <stdexcept>
#include <unistd.h>
Table::Table(const std::vector<Column> &cs, db_t *db) : columns_(cs), db(db) {}

Table::~Table() {
  db_close(db);
  db = nullptr;
}

int Table::getValue(void *primary_key, void *value, size_t buffer_size) const {
  auto err = db_search(db, &primary_key, value, buffer_size);
  return err;
}

DataType Table::keyType() const {
  return columns_[primary_key_index].data_type;
}

DataType Table::valueType(const std::wstring &column_name) const {
  for (auto &c : columns_) {
    if (c.column_name == column_name) {
      return c.data_type;
    }
  }
  throw std::runtime_error("column name not found");
}

size_t Table::entrySize() const {
  size_t size = 0;
  for (auto &c : columns_) {
    size += c.data_type.size;
  }
  return size;
}

int Table::insertValue(void *primary_key, void *value,
                       size_t value_size) const {
  return db_insert(db, primary_key, value, value_size);
}

int Table::deleteValue(void *primary_key) const {
  return db_delete(db, primary_key);
}

DataBase::DataBase(const std::string &path) {
  if (access(path.c_str(), F_OK) == 0) {
    // read the db
    std::cout << "opendb:" << path << std::endl;
  } else {
    // create the db
    int fd = open(path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0664);
    if (fd == -1) {
      throw std::runtime_error("create db failed");
    }

    std::cout << "createdb:" << path << std::endl;
  }
}

DataBase::~DataBase() {
  for (auto t : db_tables) {
    delete t;
  }
  db_tables.clear();
}
