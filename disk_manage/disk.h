#ifndef DISK_H
#define DISK_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BLOCK_NUM 4611 // 磁盘中块的总数
// 组描述符1+两个位图2+索引节点块512+数据块4096
// 从0开始计数

#define BLOCK_SIZE 512 // 块大小为512字节
#define DATA_BLOCK_START 515 // 数据块起始位置，从0开始计数，单位为块
#define DISK_NAME "../FS.txt" // 模拟磁盘的文件

// 格式化磁盘
void format_disk();

// disk指向文件描述符
void to_group_desc();

// 根据特定的索引结点号，把disk移动到相应的索引结点位置
void to_inode(uint16_t inode_No, uint16_t block_offset);

/*
 * 根据offset将disk指向block或inode位图
 * uint16_t no: 块号或索引结点号
 * 返回值：表示索引结点位置的掩码
 * 掩码在索引结点位置的比特为1，其余为0
 */
uint8_t to_bitmap(uint16_t block_offset, uint16_t no);

/*
 * 根据特定的块号，把disk移动到相应块的位置
 * 块号从0开始计数
 */
void to_block(uint16_t block_No);

/*
 * 文件指针移动到密码所在的位置
 * 为了方便，密码在组描述符后
 * 第一个字节是密码长度
 * 后面是密码内容
 */
void to_passwd();

#endif // !DISK_H
