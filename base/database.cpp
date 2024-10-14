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
    db_path = path;
  } else {
    int fd = open(path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0664);
    if (fd == -1) {
      throw std::runtime_error("create db failed");
    }
    // create the db
    std::cout << "createdb:" << path << std::endl;
    db_path = path;
  }
}

DataBase::~DataBase() {
  for (auto t : db_tables) {
    delete t;
  }
  db_tables.clear();
}

TableBuilder &TableBuilder::addColumn(const std::wstring &name,
                                      const DataType &type) {
  columns.push_back(Column{name, type});
  return *this;
}

TableBuilder &TableBuilder::setPrimaryKey(const std::wstring &name) {
  for (size_t i = 0; i < columns.size(); i++) {
    if (columns[i].column_name == name) {
      primary_key_index = i;
      return *this;
    }
  }
  throw std::runtime_error("primary key not found");
}

TableBuilder &TableBuilder::setForgeinKey(const std::wstring &name) {
  for (size_t i = 0; i < columns.size(); i++) {
    if (columns[i].column_name == name) {
      forgein_key_index = i;
      return *this;
    }
  }
  throw std::runtime_error("forgein key not found");
}

TableBuilder &TableBuilder::setName(const std::string &name) {
  table_name = name;
  return *this;
}

std::optional<DataType> TableBuilder::getPrimaryKeyType() const {
  if (primary_key_index.has_value()) {
    return columns[primary_key_index.value()].data_type;
  }
  return std::nullopt;
}

Table *TableBuilder::build() const {
  if (!primary_key_index.has_value() || columns.size() == 0 ||
      table_name == "" || db == nullptr) {
    throw std::runtime_error(
        "no primary key or column size == 0 or name is \"\"");
  }
  std::vector<Column> cs = columns;
  auto t = new Table(cs, db);
  t->primary_key_index = primary_key_index.value();
  t->table_name = table_name;
  if (this->forgein_key_index.has_value()) {
    t->forgein_key_index = forgein_key_index.value();
  }
  return t;
}

TableBuilder &TableBuilder::setDb(db_s *db) {
  this->db = db;
  return *this;
}