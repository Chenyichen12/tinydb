#pragma once
#include <optional>
#include <string>
#include <vector>

struct db_s;
struct DataType {
  enum Type { INT32, INT64, FLOAT, STRING, BOOL, DATETIME };
  Type type;
  size_t size;

  static DataType Int32() { return {INT32, sizeof(int32_t)}; }
  static DataType Int64() { return {INT64, sizeof(int64_t)}; }
  static DataType Float() { return {FLOAT, sizeof(float)}; }
  static DataType String(size_t size) { return {STRING, size}; }
  static DataType Bool() { return {BOOL, sizeof(bool)}; }

  bool operator==(const DataType &other) const {
    return type == other.type && size == other.size;
  }
  bool operator==(const Type &other) const { return type == other; }
  bool operator!=(const Type &other) const { return !(*this == other); }
  bool operator!=(const DataType &other) const { return !(*this == other); }

private:
  DataType(Type t, size_t s) : type(t), size(s) {}
};

struct Column {
  std::wstring column_name;
  DataType data_type;
};

class TableBuilder;
class DataBase;
/**
 * @brief the table of db
 * won't check the type of the key and value
 */
class Table {
  friend TableBuilder;
  friend DataBase;
  std::vector<Column> columns_;
  // set by DataBase
  int primary_key_index;
  int forgein_key_index;

  db_s *db;
  std::string table_name;

public:
  /**
   * @brief Construct a new Table object
   *
   * @param cs
   * @param db  take the ownership of db, one db is one table, should be created
   * and opened by DataBase
   */
  Table(const std::vector<Column> &cs, db_s *db);
  ~Table();
  /**
   * @brief Get the Value object
   * @param[in] primary_key key value
   * @param[out] value get value
   * @param[in] buffer_size value buffer size
   * @return int 0 if success, -1 if error
   */
  int getValue(void *primary_key, void *value, size_t buffer_size = 1000) const;
  int insertValue(void *primary_key, void *value, size_t value_size) const;
  int deleteValue(void *primary_key) const;

  DataType keyType() const;
  DataType valueType(const std::wstring &column_name) const;
  const std::vector<Column> &columns() const { return columns_; }
  size_t entrySize() const;
  std::string name() const { return table_name; }
};

class TableBuilder {
  std::vector<Column> columns;
  std::string table_name;
  std::optional<int> primary_key_index;
  std::optional<int> forgein_key_index;
  db_s *db = nullptr;
public:
  /**
   * @brief
   * @return Table* result
   * @throw std::runtime_error if no primary key or column size == 0 or name is
   * ""
   */
  Table *build() const;
  TableBuilder &addColumn(const std::wstring &name, const DataType &type);
  TableBuilder &setPrimaryKey(const std::wstring &name);
  TableBuilder &setForgeinKey(const std::wstring &name);
  TableBuilder &setName(const std::string &name);
  TableBuilder &setDb(db_s *db);
  std::optional<DataType> getPrimaryKeyType() const;
};

class DataBase {
  std::vector<Table *> db_tables;
  std::string db_path;

public:
  /**
   * @brief Construct a new Data Base object
   * if exist open it, else create it
   *
   * @param path
   */
  DataBase(const std::string &path);
  DataBase(const DataBase &) = delete;

  ~DataBase();
};