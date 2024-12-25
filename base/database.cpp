#include "database.h"
#include "file/store.h"
#include <fcntl.h> // for open(), O_CREAT, O_RDWR, O_TRUNC
#include <filesystem>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unistd.h>

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
    builder->setFromConfig(t);

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

  // check the foreign key is valid
  const auto &check_foreign = b->getForgeinKeys();
  for (const auto &c : check_foreign) {
    bool found = false;
    for (const auto &t : db_tables) {
      auto table_name = t->table_name;
      auto primary_string = t->columns()[t->primaryKeyIndex()].column_name;
      if (c.table_name == table_name && c.column_name == primary_string) {
        found = true;
        break;
      }
    }
    if (!found) {
      return 6;
    }
  }

  // prepare to build table
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
    j["tables"].push_back(t->getConfig());
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

int DataBase::insertValue(const std::string &table_name, void *primary_key,
                          void *value) {
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

  auto res = target_table->insertValue(primary_key, value,
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

int DataBase::getValue(
    const std::string &table_name, void *primary_key,
    const std::function<void(RowReader *reder)> &callback) const {
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
  auto val = std::make_unique<char[]>(target_table->entrySize());
  auto res =
      target_table->getValue(primary_key, val.get(), target_table->entrySize());
  if (res == 0) {
    auto reader =
        std::make_unique<RowReader>(target_table->columns(), val.get());
    callback(reader.get());
    return 0;
  }
  return 2;
}

bool DataBase::tableExist(const std::string &table_name) const {
  for (const auto &t : db_tables) {
    if (t->name() == table_name) {
      return true;
    }
  }
  return false;
}
const Table *DataBase::getTable(const std::string &table_name) const {
  for (const auto &t : db_tables) {
    if (t->name() == table_name) {
      return t;
    }
  }
  return nullptr;
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
size_t RowReader::byteSize() const {
  size_t size = 0;
  for (const auto &c : columnsDefination) {
    size += c.data_type.size;
  }
  return size;
}

void RowReader::readByte(char *buf, size_t bufSize, size_t offset) const {
  // memcpy(buf, value, bufSize);

  memcpy(buf + offset, value, bufSize);
}

bool RowReader::operator=(const RowReader &r) const {
  return memcmp(value, r.value, byteSize());
}

RowReader *RowReader::clone() const {
  auto new_value = new char[byteSize()];
  memcpy(new_value, value, byteSize());

  auto columnsDefination = new std::vector<Column>(this->columnsDefination);

  auto reader =  new RowReader(*columnsDefination, new_value);
  reader->hasOnwer = true;
  return reader;
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

int RowReader::columnAt(const std::string &colName) const {
  for (size_t i = 0; i < columnsDefination.size(); i++) {
    if (columnsDefination[i].column_name == colName) {
      return i;
    }
  }
  return -1;
}

RowReader::~RowReader() {
  if (hasOnwer) {
    delete[] value;
    delete &columnsDefination;
  }
};