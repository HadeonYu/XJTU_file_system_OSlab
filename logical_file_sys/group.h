#ifndef GROUP_H
#define GROUP_H

#include "../disk_manage/disk.h"
#include "logical_struct.h"
#include <stdint.h>

// 一个目录的信息，用在load_dir_entry函数
typedef struct dir_info_t {
  dir_entry_t *dir;   // 目录项数组
  uint16_t dir_count; // 目录项数量
} dir_info_t;

// 格式化组
void format_memory();

// 初始化
void initial_memory();

// 把组描述符更新到硬盘
void update_group_desc();

// 载入组描述符
void reload_group_desc();

// 根据inode读取文件的数据块
block_t *read_blocks(inode_t *inode);

/*
 * 根据inode号更新文件的数据块
 * 内容和read_blocks对称，
 * 确保对*blocks数组中元素顺序和read_blocks中的相同
 */
void update_blocks(inode_t *inode, block_t **blocks);

/*
 * 向给索引结点加入块：修改block和blocks_count字段
 * new_blocks_count: 新加入的块的数量，也就是*blocks_No的长度
 */
void add_blocks(inode_t *inode, uint16_t **blocks_No,
                uint16_t new_blocks_count);

// 释放一个索引结点的所有数据块
void free_blocks(inode_t *inode);

// 读取目录项
dir_info_t load_dir_entry(uint16_t inode_No);

// 创建目录项
dir_entry_t create_dir_entry(const char *name, uint16_t inode_No,
                             uint8_t file_type);

// 更新目录项
void update_dir_entry(uint16_t inode_No, dir_entry_t *dirs);

// 移动目录项指针到当前目录项下一个目录项
void move_dir_entry(dir_entry_t **dir);

#endif // !GROUP_H
