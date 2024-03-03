#ifndef BLOCK_H
#define BLOCK_H

#include "../disk_manage/disk.h"
#include "logical_struct.h"
#include <stdint.h>

// 分配模式，在alloc_inode_block()用到
typedef enum alloc_mod {
  INODE, // 分配索引结点
  BLOCK, // 分配块
} alloc_mod;

// 根据索引结点号载入索引结点
inode_t load_inode_entry(uint16_t inode_No);

// 根据索引结点号更新索引结点
void update_inode_entry(uint16_t inode_No, inode_t *inode);

// 根据块号载入块
block_t load_block_entry(uint16_t block_No);

/*
 * 多级索引加载块
 * blocks: 保存块的数组，需要在调用函数时分配空间并传入地址
 * cur_count: blocks 中当前已处理块的数量，也是*block数组已处理的长度
 * total_count: 需要加载的块的总数，*block数组的长度
 * block_No: 块号
 * level: 索引级别，0为直接索引
 */
void load_block_muti_level(block_t **blocks, uint16_t *cur_count,
                           uint16_t total_count, uint16_t block_No,
                           uint16_t level);

// 更新块
void update_block_entry(uint16_t block_No, block_t *block);

/*
 * 多级索引更新块，参数和多级索引加载块一样
 * 内容和load_block_muti_level完全对称，
 * 确保在read和write时，同一文件的*block数组顺序相同
 */
void update_block_muti_level(block_t **blocks, uint16_t *cur_count,
                             uint16_t total_count, uint16_t block_No,
                             uint16_t level);

/*
 * 分配一个索引结点或一个数据块，返回索引结点或块号
 * 参数 mod 表示分配模式
 */
uint16_t alloc_inode_block(alloc_mod mod);

// 删除索引结点或数据块
void free_inode_block(uint16_t No, alloc_mod mod);

#endif // !BLOCK_H
