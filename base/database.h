#pragma once
#include <functional>
#include <string>
#include <vector>
#include "table.h"


class RowBuilder {
  const std::vector<Column> &columnsDefination;
  // may be need to use a two demensional vector
  char *comp_value;
  size_t offset = 0;
  bool checkType(DataType::Type type) const;
  void doMemoryCopy(const void *value, size_t size);

public:
  RowBuilder(const std::vector<Column> &columnsDefination);
  // sequence add
  RowBuilder &addValue(int32_t value);
  RowBuilder &addValue(int64_t value);
  RowBuilder &addValue(float value);
  RowBuilder &addValue(const std::wstring &value);
  RowBuilder &addValue(const std::string &value);
  RowBuilder &addValue(bool value);
  bool isAllSet() const;
  size_t valueOffset(int index) const;
  const char *value() const { return comp_value; }
  ~RowBuilder();
};

class RowReader {
  const std::vector<Column> &columnsDefination;
  const char *value;
  void read(size_t index, void *buffer, size_t size) const;

public:
  RowReader(const std::vector<Column> &columnsDefination, const char *value);

  /**
   * @brief if the type is not match, throw the error
   * @param index column index
   * @return data value
   */
  std::wstring readString(int index) const;
  int32_t readInt32(int index) const;
  int64_t readInt64(int index) const;
  float readFloat(int index) const;
  bool readBool(int index) const;
  ~RowReader();
};

class DataBase {
  std::vector<Table *> db_tables;
  std::string db_path;
  int db_fd = -1;
  /**
   * @brief
   * may throw std::runtime_error if read failed
   */
  void readConfig();

public:
  /**
   * @brief Construct a new Data Base object
   * if exist open it, else create it
   *
   * @param path
   */
  DataBase(const std::string &path);
  DataBase(const DataBase &) = delete;
  /**
   * @brief
   *
   * @param callback add the table columns and keys to table
   * @return int error code, 0 if success
   * 1 if no primary key
   * 2 if no name
   * 3 if no key is not valid type
   * 4 if db is not set
   * 5 if table name is already exist
   * 6 if foreign key is not valid
   */
  int addTable(const std::function<void(TableBuilder *b)> &callback);

  /**
   * @brief insert the value
   *
   * @param table_name which table
   * @param callback insert the value in this callback
   * @return int error code
   * 0 if success
   * 1 if table not found
   * 2 if the value is not complete
   * 3 if insert failed may depliacte key
   */
  int insertValue(const std::string &table_name,
                  const std::function<void(RowBuilder *r)> &callback);

/**
 * @brief Get the Value object
 * 
 * @param table_name 
 * @param callback 
 * @return int error code
  * 0 if success
  * 1 if table not found
  * 2 if the value is not complete
 */
  int getValue(const std::string &table_name,
               const std::function<void(RowReader *reader)> &callback) const;
/**
 * @brief Get the Value object
 * 
 * @param table_name 
 * @param callback 
 * @return int error code
  * 0 if success
  * 1 if table not found
  * 2 if get value is not success may be not found
 */
  int getValue(const std::string &table_name, void *primary_key,
               const std::function<void(RowReader *reder)>& callback) const;

  bool tableExist(const std::string &table_name) const;
  const Table* getTable(const std::string &table_name) const;

  void saveConfig() const;

  ~DataBase();
};