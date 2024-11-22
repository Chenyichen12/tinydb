#include "table.h"
#include "file/store.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
Table::Table(const std::vector<Column> &cs, db_t *db) : columns_(cs), db(db) {}

Table::~Table() {
  db_close(db);
  db = nullptr;
}

int Table::getValue(void *primary_key, void *value, size_t buffer_size) const {
  auto err = db_search(db, primary_key, value, buffer_size);
  return err;
}

bool Table::hasColumn(const std::string &name) const {
  for (const auto &c : columns_) {
    if (c.column_name == name) {
      return true;
    }
  }
  return false;
}

int Table::getColumnIndex(const std::string &name) const {
  for (size_t i = 0; i < columns_.size(); i++) {
    if (columns_[i].column_name == name) {
      return i;
    }
  }
  return -1;
}

int Table::getValue(
    const std::function<void(void *key, void *value)> &callback) const {
  auto err = db_check_all(db, callback);
  return err;
}

DataType Table::keyType() const {
  return columns_[primary_key_index].data_type;
}

DataType Table::valueType(const std::string &column_name) const {
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

TableBuilder &TableBuilder::addColumn(const std::string &name,
                                      const DataType &type) {
  columns.push_back(Column{name, type});
  return *this;
}

TableBuilder &TableBuilder::setPrimaryKey(int index) {
  primary_key_index = index;
  return *this;
}
TableBuilder &TableBuilder::setPrimaryKey(const std::string &name) {
  for (size_t i = 0; i < columns.size(); i++) {
    if (columns[i].column_name == name) {
      primary_key_index = i;
      return *this;
    }
  }
  throw std::runtime_error("primary key not found");
}

TableBuilder &TableBuilder::setForgeinKey(const std::string &name,
                                          const std::string &table,
                                          const std::string &column) {
  for (size_t i = 0; i < columns.size(); i++) {
    if (columns[i].column_name == name) {
      forgein_keys.push_back(ForeignKey{i, table, column});
      return *this;
    }
  }
  throw std::runtime_error("forgein key not found");
}
TableBuilder &TableBuilder::setForgeinKey(size_t index,
                                          const std::string &table,
                                          const std::string &column) {
  if (index >= columns.size()) {
    throw std::runtime_error("index out of range");
  }
  forgein_keys.push_back(ForeignKey{index, table, column});
  return *this;
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

  t->forgein_keys = forgein_keys;
  return t;
}

TableBuilder &TableBuilder::setDb(db_s *db) {
  this->db = db;
  return *this;
}

nlohmann::json Table::getConfig() const {
  nlohmann::json table;
  table["name"] = name();
  table["primary_key_index"] = primaryKeyIndex();
  table["forgein_keys"] = nlohmann::json::array();
  for (const auto &fk : forgeinKeys()) {
    auto fk_json = nlohmann::json::object();
    fk_json["index"] = fk.index;
    fk_json["column"] = fk.column_name;
    fk_json["table"] = fk.table_name;
    table["forgein_keys"].push_back(fk_json);
  }

  table["columns"] = nlohmann::json::array();
  for (const auto &c : columns()) {
    nlohmann::json column;
    column["name"] = c.column_name;
    column["type"] = c.data_type.type;
    column["size"] = c.data_type.size;
    table["columns"].push_back(column);
  }
  return table;
}

TableBuilder &TableBuilder::setFromConfig(const nlohmann::json &j) {
  setName(j["name"]);
  setPrimaryKey(j["primary_key_index"].get<int>());
  for (const auto &fk : j["forgein_keys"]) {
    setForgeinKey(fk["index"].get<int>(), fk["table"], fk["column"]);
  }
  auto columns = j["columns"].get<std::vector<nlohmann::json>>();
  for (const auto &c : columns) {
    int type = c["type"];
    switch (type) {
    case 0:
      addColumn(c["name"], DataType::Int32());
      break;
    case 1:
      addColumn(c["name"], DataType::Int64());
      break;
    case 2:
      addColumn(c["name"], DataType::Float());
      break;
    case 3:
      addColumn(c["name"], DataType::String(c["size"]));
      break;
    case 4:
      addColumn(c["name"], DataType::Bool());
      break;
    default:
      throw std::runtime_error("unknow type");
    }
  }
  return *this;
}