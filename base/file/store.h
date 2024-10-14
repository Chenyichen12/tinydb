#pragma once

#include <cstddef>
#include <sys/types.h>

/**
 * @brief 文件数据库的头，即是句柄
 */
typedef struct db_s{
    int fd;                             /** 文件句柄 */
    int key_type;                       /** key类型，必须在创建文件数据库时指定 */
    size_t key_size;                    /** key的最大长度 */
    size_t key_align;                   /** 对齐，值 = db_align(sizeof(btree_key) + key_size, DB_ALIGNMENT) */
    size_t M;                           /** Btree 节点child的最大值 */
    size_t key_total;                   /** 已存储的key总数 */
    size_t key_use_block;               /** 数据块为btree_key类型的总数 */
    size_t value_use_block;             /** 数据块为btree_value类型的总数 */ 
    off_t free;                         /** 空闲链表的头 */
    off_t current;                      /** 当前作为btree_value的数据块，未用完分配空间 */
    int (*key_cmp)(void*,void*,size_t); /** key比较方式 */
}db_t;

enum KEY_TYPE{
    DB_STRINGKEY,
    DB_BYTESKEY,
    DB_INT32KEY,
    DB_INT64KEY
};


int db_create(const char *path, KEY_TYPE key_type, size_t max_key_size);
int db_open(db_t **db, const char *path);
/**
 * @brief insert key 插入值
 * @param[in] db 数据库句柄
 * @param[in] key key_size can't exceed max_key_size 需要保证key类型和创建数据库时一致
 * @param[in] value
 * @param[in] value_size value_size can't too large 不能太大
 * @return ==1 if success, ==0 if key repeat, ==-1 error
*/
int db_insert(db_t* db, void* key, void *value, size_t value_size);
/**
 * @brief search key 查询值
 * @param[in] db 数据库句柄
 * @param[in] key key_size can't exceed max_key_size 需要保证key类型和创建数据库时一致
 * @param[out] value
 * @param[in] value_size 需要保证空间足够大
 * @return >=0 if success, ==-1 error
*/
int db_search(db_t* db, void* key, void *value, size_t value_size);
/**
 * @brief delete key 删除值
 * @param[in] db 数据库句柄
 * @param[in] key key_size can't exceed max_key_size 需要保证key类型和创建数据库时一致
 * @return ==1 if success, ==0 if key no found, ==-1 error
*/
int db_delete(db_t* db, void* key);
void db_close(db_t *db);

int db_check_all(db_t *db, void (*callback)(void* key, void* value), size_t value_size);