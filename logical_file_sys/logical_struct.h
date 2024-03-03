#ifndef LOGICAL_STRUCT_H
#define LOGICAL_STRUCT_H

/*硬盘结构（只有一个组）
 *
 * 组描述符   数据块位图    索引节点位图    索引节点表    数据块
 * 1 block    1 block       1 block         512 block     4096 block
 * 512B       512B          512B            256KB         2MB
 */

#include "../disk_manage/disk.h"
#include <stdint.h>

#define DATA_BLOCK_NUM 4096; // 存数据的块的个数，从0开始计数
#define INODE_NUN 4096 // inode节点的数量，索引节点从1开始计数，0表示NULL
#define FILE_NAME_LEN 256 // 文件名最长255，最后一个结束符
#define DIR_ENTRY_BASIC_SIZE 6 // dir_entry_t 除文件名外字段的大小，单位字节

/*
 * inode 结构体
 * 占64字节
 */
/*
 * 关于i_mode:
 * 高8位是文件类型编码的拷贝，1:普通文件; 2:目录
 * 低3位为rwx3个属性，所有对象共用
 * 中间5位填0
 */
/*
 * 关于i_block:
 * 前6个直接索引，指向存放数据的数据块
 * 第7个一级索引，指向存放索引节点的数据块
 * 第8个为二级索引
 * 多级索引中，2字节记录一个块号，一个数据块可记录512/2=256个块
 */
typedef struct inode_t {
  uint16_t i_mode;         // 文件类型和访问权限
  uint16_t i_blocks_count; // 文件的数据块个数
  uint32_t i_size;         // 大小（字节）
  uint64_t i_atime;        // 访问时间
  uint64_t i_ctime;        // 创建时间
  uint64_t i_mtime;        // 修改时间
  uint64_t i_dtime;        // 删除时间
  uint16_t i_block[8];     // 指向数据块的指针
  char i_pad[8];           // 填充
} inode_t;

// 目录和文件结构体
/*关于目录项长度
 * 目录项长度记录的是当前目录项占的字节数
 * （包括name字符数组最后的'\0'）
 */

/*文件类型
 * 1: 普通文件
 * 2: 目录
 */
typedef struct dir_entry_t {
  uint16_t inode;           // 索引节点号
  uint16_t rec_len;         // 目录项长度
  uint8_t name_len;         // 文件名长度
  uint8_t file_type;        // 文件类型
  char name[FILE_NAME_LEN]; // 文件名最大255字符，最后一个结束符
} dir_entry_t;

/*
 * 超级块和组描述符的简化合并
 * 占32字节，用一个块
 * 文件系统只有一个组
 */
typedef struct group_desc_t {
  char bg_volume_name[16];       // 卷名
  uint16_t bg_block_bitmap;      // 保存块位图的块号
  uint16_t bg_inode_bitmap;      // 保存索引节点位图的块号
  uint16_t bg_inode_table;       // 索引节点表的起始块号
  uint16_t bg_free_blocks_count; // 本组空闲块个数
  uint16_t bg_free_inodes_count; // 本组空闲索引节点个数
  uint16_t bg_used_dirs_count;   // 本组目录的个数
  char bg_pad[4];                // 填充(0xff)
} group_desc_t;

typedef enum FILE_TYPE {
  UNKNOWN,
  COMMON,
  DIR,
} FILE_TYPE;

// 块结构体，大小512B
typedef struct block_t {
  uint8_t data[BLOCK_SIZE];
} block_t;

#endif // !LOGICAL_STRUCT_H
