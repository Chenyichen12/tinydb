#include "database.h"
#include "file/store.h"
#include <fcntl.h> // for open(), O_CREAT, O_RDWR, O_TRUNC
#include <filesystem>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
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

    db_fd = fd;
    db_path = path;
    saveConfig();
  }
}

DataBase::~DataBase() {
  for (auto t : db_tables) {
    delete t;
  }
  db_tables.clear();
  if (db_fd != -1) {
    close(db_fd);
  }
}

int DataBase::addTable(const std::function<void(TableBuilder *b)> &callback) {
  auto b = std::make_unique<TableBuilder>();
  callback(b.get());
  if (!b->getPrimaryKeyType()) {
    return 1;
  }
  if (b->name() == "") {
    return 2;
  }
  for (const auto &t : this->db_tables) {
    if (t->name() == b->name()) {
      return 5;
    }
  }

  db_s *table = nullptr;
  std::filesystem::path fs(db_path);
  auto tablePath = fs.parent_path() / (b->name() + ".db");
  KEY_TYPE t;
  auto type = b->getPrimaryKeyType().value().type;
  switch (type) {
  case DataType::Type::INT32:
    t = DB_INT32KEY;
    break;
  case DataType::Type::INT64:
    t = DB_INT64KEY;
    break;
  case DataType::Type::STRING:
    t = DB_STRINGKEY;
    break;
  default:
    return 3;
  }

  auto res =
      db_create(tablePath.c_str(), t, b->getPrimaryKeyType().value().size);
  if (res == -1) {
    return 4;
  }
  db_open(&table, tablePath.c_str());
  b->setDb(table);
  try {
    auto table_ = b->build();
    db_tables.push_back(table_);
  } catch (std::exception &e) {
    db_close(table);
    unlink(tablePath.c_str());
    throw e;
  }

  return 0;
}

void DataBase::saveConfig() const {
  nlohmann::json j;
  j["db_name"] = db_path;
  j["tables"] = nlohmann::json::array();
  for (const auto &t : db_tables) {
    nlohmann::json table;
    table["name"] = t->name();
    table["primary_key_index"] = t->primaryKeyIndex();
    table["forgein_key_index"] = t->forgeinKeyIndex();
    table["columns"] = nlohmann::json::array();
    for (const auto &c : t->columns()) {
      nlohmann::json column;
      column["name"] = c.column_name;
      column["type"] = c.data_type.type;
      column["size"] = c.data_type.size;
      table["columns"].push_back(column);
    }
    j["tables"].push_back(table);
  }

  // 清除原有的配置文件
  if (ftruncate(db_fd, 0) == -1) {
    throw std::runtime_error("ftruncate failed");
  }
  if (lseek(db_fd, 0, SEEK_SET) == -1) {
    throw std::runtime_error("lseek failed");
  }

  auto string = nlohmann::to_string(j);
  if (write(db_fd, string.c_str(), string.size()) == -1) {
    throw std::runtime_error("write failed");
  }
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