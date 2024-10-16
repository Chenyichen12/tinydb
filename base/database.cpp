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

DataBase::DataBase(const std::string &path) {
  if (access(path.c_str(), F_OK) == 0) {
    // read the db
    std::cout << "opendb:" << path << std::endl;
    db_path = path;
    int fd = open(path.c_str(), O_RDWR);

    if (fd == -1) {
      throw std::runtime_error("open db failed");
    }
    db_fd = fd;

    readConfig();
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

void DataBase::readConfig() {
  auto buf = std::make_unique<char[]>(4096);
  pread(db_fd, buf.get(), 4096, 0);
  nlohmann::json j = nlohmann::json::parse(buf.get());

  auto array = j["tables"].get<std::vector<nlohmann::json>>();
  for (const auto &t : array) {

    auto builder = std::make_unique<TableBuilder>();
    builder->setName(t["name"]);
    builder->setPrimaryKey(t["primary_key_index"].get<int>());
    int forgein_key_index = t["forgein_key_index"];
    if (forgein_key_index != -1) {
      builder->setForgeinKey(t["forgein_key_index"].get<int>());
    }

    auto columns = t["columns"].get<std::vector<nlohmann::json>>();
    for (const auto &c : columns) {
      int type = c["type"];
      switch (type) {
      case 0:
        builder->addColumn(c["name"], DataType::Int32());
        break;
      case 1:
        builder->addColumn(c["name"], DataType::Int64());
        break;
      case 2:
        builder->addColumn(c["name"], DataType::Float());
        break;
      case 3:
        builder->addColumn(c["name"], DataType::String(c["size"]));
        break;
      case 4:
        builder->addColumn(c["name"], DataType::Bool());
        break;
      default:
        throw std::runtime_error("unknow type");
      }
    }

    std::filesystem::path fs(db_path);
    auto tablePath = fs.parent_path() / (builder->name() + ".db");
    db_s *table = nullptr;
    db_open(&table, tablePath.c_str());
    builder->setDb(table);
    auto table_ = builder->build();
    db_tables.push_back(table_);
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

int DataBase::insertValue(const std::string &table_name,
                          const std::function<void(RowBuilder *r)> &callback) {
  Table *target_table = nullptr;
  for (auto t : db_tables) {
    if (t->name() == table_name) {
      target_table = t;
      break;
    }
  }

  if (target_table == nullptr) {
    return 1;
  }

  auto builder = std::make_unique<RowBuilder>(target_table->columns());
  callback(builder.get());
  if (!builder->isAllSet()) {
    return 2;
  }

  auto valsize = target_table->entrySize();
  auto v = std::make_unique<char[]>(valsize);
  memcpy(v.get(), builder->value(), valsize);

  auto keyindex = target_table->primaryKeyIndex();
  auto keysize = target_table->keyType().size;
  auto key_valueoffset = builder->valueOffset(keyindex);
  auto keyval = std::make_unique<char[]>(keysize);
  memcpy(keyval.get(), v.get() + key_valueoffset, keysize);
  auto res = target_table->insertValue(keyval.get(), v.get(),
                                       target_table->entrySize());
  if (res == 1) {
    return 0;
  }

  return 3;
}

int DataBase::getValue(
    const std::string &table_name,
    const std::function<void(RowReader *reader)> &callback) const {
  Table *target_table = nullptr;
  for (auto t : db_tables) {
    if (t->name() == table_name) {
      target_table = t;
      break;
    }
  }
  if (target_table == nullptr) {
    return 1;
  }

  auto valsize = target_table->entrySize();
  auto v = std::make_unique<char[]>(valsize);
  auto reader = std::make_unique<RowReader>(target_table->columns(), v.get());

  auto res = target_table->getValue([&](void *key, void *value) {
    memcpy(v.get(), value, valsize);
    callback(reader.get());
  });

  if (res == 0) {
    return 0;
  }
  return 2;
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
TableBuilder &TableBuilder::setForgeinKey(int index) {
  forgein_key_index = index;
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

TableBuilder &TableBuilder::setForgeinKey(const std::string &name) {
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

RowBuilder::RowBuilder(const std::vector<Column> &columnsDefination)
    : columnsDefination(columnsDefination) {
  auto totalSize = 0;
  for (auto &c : columnsDefination) {
    totalSize += c.data_type.size;
  }
  comp_value = new char[totalSize];
}
void RowBuilder::doMemoryCopy(const void *value, size_t size) {
  auto byteoffset = 0;
  for (size_t i = 0; i < offset; i++) {
    byteoffset += columnsDefination[i].data_type.size;
  }
  memcpy(comp_value + byteoffset, value, size);
}
bool RowBuilder::checkType(DataType::Type type) const {
  auto &c = columnsDefination[offset];
  return c.data_type.type == type;
}
RowBuilder &RowBuilder::addValue(int32_t value) {
  // check
  auto res = checkType(DataType::INT32);
  if (!res) {
    throw std::runtime_error("type not match");
  }

  doMemoryCopy(&value, sizeof(int32_t));
  offset++;
  return *this;
}
RowBuilder &RowBuilder::addValue(int64_t value) {
  // check
  auto res = checkType(DataType::INT64);
  if (!res) {
    throw std::runtime_error("type not match");
  }
  doMemoryCopy(&value, sizeof(int64_t));
  offset++;
  return *this;
}
RowBuilder &RowBuilder::addValue(float value) {
  auto res = checkType(DataType::FLOAT);
  if (!res) {
    throw std::runtime_error("type not match");
  }
  doMemoryCopy(&value, sizeof(float));
  offset++;
  return *this;
}
RowBuilder &RowBuilder::addValue(const std::wstring &value) {
  auto res = checkType(DataType::STRING);
  if (!res) {
    throw std::runtime_error("type not match");
  }
  auto stringsize = columnsDefination[offset].data_type.size;
  size_t size_in_bytes = value.size() * sizeof(wchar_t);
  if (size_in_bytes > stringsize) {
    throw std::runtime_error("string size too large");
  }

  doMemoryCopy(value.c_str(), size_in_bytes);
  offset++;

  return *this;
}
RowBuilder &RowBuilder::addValue(const std::string &value) {
  auto converter = std::wstring_convert<std::codecvt_utf8<wchar_t>>();
  auto wstr = converter.from_bytes(value);
  return addValue(wstr);
}
RowBuilder &RowBuilder::addValue(bool value) {
  auto res = checkType(DataType::BOOL);
  if (!res) {
    throw std::runtime_error("type not match");
  }
  doMemoryCopy(&value, sizeof(bool));
  offset++;
  return *this;
}

RowBuilder::~RowBuilder() { delete[] comp_value; }

bool RowBuilder::isAllSet() const { return offset == columnsDefination.size(); }

size_t RowBuilder::valueOffset(int index) const {
  auto byteoffset = 0;
  for (size_t i = 0; i < index; i++) {
    byteoffset += columnsDefination[i].data_type.size;
  }
  return byteoffset;
}

RowReader::RowReader(const std::vector<Column> &columnsDefination,
                     const char *value)
    : columnsDefination(columnsDefination), value(value) {}

void RowReader::read(size_t index, void *buffer, size_t size) const {
  auto byteoffset = 0;
  for (size_t i = 0; i < index; i++) {
    byteoffset += columnsDefination[i].data_type.size;
  }
  memcpy(buffer, value + byteoffset, size);
}

std::wstring RowReader::readString(int index) const {
  auto &c = columnsDefination[index];
  if (c.data_type.type != DataType::STRING) {
    throw std::runtime_error("type not match");
  }
  auto stringsize = c.data_type.size;
  auto buffer = new wchar_t[stringsize];
  read(index, buffer, stringsize);
  std::wstring wstr(buffer);
  delete[] buffer;
  return wstr;
}
int32_t RowReader::readInt32(int index) const {
  auto &c = columnsDefination[index];
  if (c.data_type.type != DataType::INT32) {
    throw std::runtime_error("type not match");
  }
  int32_t value;
  read(index, &value, sizeof(int32_t));
  return value;
}
int64_t RowReader::readInt64(int index) const {
  auto &c = columnsDefination[index];
  if (c.data_type.type != DataType::INT64) {
    throw std::runtime_error("type not match");
  }
  int64_t value;
  read(index, &value, sizeof(int64_t));
  return value;
}
float RowReader::readFloat(int index) const {
  auto &c = columnsDefination[index];
  if (c.data_type.type != DataType::FLOAT) {
    throw std::runtime_error("type not match");
  }
  float value;
  read(index, &value, sizeof(float));
  return value;
}
bool RowReader::readBool(int index) const {
  auto &c = columnsDefination[index];
  if (c.data_type.type != DataType::BOOL) {
    throw std::runtime_error("type not match");
  }
  bool value;
  read(index, &value, sizeof(bool));
  return value;
}

RowReader::~RowReader() = default;