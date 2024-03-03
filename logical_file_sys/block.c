#include "block.h"
#include "group.h"
#include "logical_struct.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint16_t last_alloc_block; // 上次分配的数据块号
uint16_t last_alloc_inode; // 上次分配的索引结点号

// from disk.c:
extern FILE *disk;

// from group.c:
extern group_desc_t group_desc; // 组描述符

inode_t load_inode_entry(uint16_t inode_No) {
  to_inode(inode_No, group_desc.bg_inode_table);
  inode_t inode;
  fread(&inode, sizeof(inode_t), 1, disk);
  return inode;
}

void update_inode_entry(uint16_t inode_No, inode_t *inode) {
  if (inode_No < 1) {
    printf("invalid inode_No\n");
    return;
  }
  to_inode(inode_No, group_desc.bg_inode_table);
  fwrite(inode, sizeof(inode_t), 1, disk);
}

block_t load_block_entry(uint16_t block_No) {
  to_block(block_No);
  block_t block;
  fread(&block, sizeof(block_t), 1, disk);
  return block;
}

void load_block_muti_level(block_t **blocks, uint16_t *cur_count,
                           uint16_t total_count, uint16_t block_No,
                           uint16_t level) {
  if (level < 0 || level > 2) {
    printf("invalid level: %d\n", level);
    return;
  }
  if (level == 0) {
    // 直接索引，递归终止条件
    (*blocks)[*cur_count] = load_block_entry(block_No);
    (*cur_count)++;
    return;
  } else {
    // 多级索引
    // 数据块中存的是块号，一个块号占2字节
    block_t sub_block = load_block_entry(block_No);
    uint16_t data_block_No[sizeof(sub_block.data) /
                           sizeof(uint16_t)]; // 下一级数据块的块号
    memset(data_block_No, 0, sizeof(sub_block.data));
    memcpy(data_block_No, &sub_block.data, sizeof(sub_block.data));
    for (int i = 0; i < sizeof(sub_block.data) / sizeof(uint16_t); i++) {
      if (data_block_No[i] == 0) {
        break;
      }
      data_block_No[i] = ntohs(((uint16_t *)sub_block.data)[i]);
    }
    int itera = 0;
    while (data_block_No[itera] != 0) { // 只有根目录占用的块号为0
      load_block_muti_level(blocks, cur_count, total_count,
                            data_block_No[itera], level - 1);
      itera++;
    }
  }
}

void update_block_entry(uint16_t block_No, block_t *block) {
  to_block(block_No);
  fwrite(block, sizeof(block_t), 1, disk);
}

void update_block_muti_level(block_t **blocks, uint16_t *cur_count,
                             uint16_t total_count, uint16_t block_No,
                             uint16_t level) {
  if (level < 0 || level > 2) {
    printf("invalid level: %d\n", level);
    return;
  }
  if (level == 0) {
    // 直接索引，递归终止条件
    update_block_entry(block_No, *blocks + *cur_count);
    (*cur_count)++;
    return;
  } else {
    // 多级索引
    // 数据块中存的是块号，一个块号占2字节
    block_t sub_block = load_block_entry(block_No);
    uint16_t data_block_No[sizeof(sub_block.data) /
                           sizeof(uint16_t)]; // 下一级数据块的块号
    memset(data_block_No, 0, sizeof(sub_block.data));
    memcpy(data_block_No, &sub_block.data, sizeof(sub_block.data));
    for (int i = 0; i < sizeof(sub_block.data) / sizeof(uint16_t); i++) {
      if (data_block_No[i] == 0) {
        break;
      }
      data_block_No[i] = ntohs(((uint16_t *)sub_block.data)[i]);
    }
    int itera = 0;
    while (data_block_No[itera] != 0) { // 只有根目录占用的块号为0
      update_block_muti_level(blocks, cur_count, total_count,
                              data_block_No[itera], level - 1);
      itera++;
    }
  }
}

uint16_t alloc_inode_block(alloc_mod mod) {
  uint8_t map_byte;       // 位图中的一个字节
  uint8_t find_free_flag; // 0表示找到空闲结点
  int block_offset; // 相对位图块起始地址的偏移量，单位为字节
  // 从上一次分配点开始找空闲块
  if (mod == INODE) {
    if (group_desc.bg_free_inodes_count == 0) {
      printf("there exists no free inode");
      return 0;
    }
    to_bitmap(group_desc.bg_inode_bitmap, last_alloc_inode - 1);
    block_offset = (last_alloc_inode - 1) / 8; // 减1是因为索引结点从1开始计数
  } else if (mod == BLOCK) {
    if (group_desc.bg_free_blocks_count == 0) {
      printf("there exists no free block");
      return -1;
    }
    to_bitmap(group_desc.bg_block_bitmap, last_alloc_block);
    block_offset = last_alloc_block / 8;
  } else {
    printf("alloc mod error\n");
    return -1;
  }
  uint8_t mask; // 掩码，和map_byte与操作来判断是否空闲
  uint8_t byte_offset = 0; // mask右移的次数，用于计算索引结点号，单位为比特
  for (; block_offset < BLOCK_SIZE; block_offset++, byte_offset = 0) {
    fread(&map_byte, sizeof(uint8_t), 1, disk);
    mask = 0x80;
    while (mask) {
      find_free_flag = mask & map_byte;
      // map_byte中0表示空闲，1表示占用
      if (!find_free_flag) {
        // flag为0表示找到这一位空闲
        break;
      }
      mask >>= 1;
      byte_offset++;
    }
    if (!find_free_flag) {
      break;
    }
  }
  if (find_free_flag) {
    // 没找到空闲索引结点，返回0
    return 0;
  }
  // 找到空闲索引结点
  // 修改位图
  map_byte |= mask;
  fseek(disk, -sizeof(uint8_t), SEEK_CUR); // 向前移动一字节
  // 写回map_byte
  fwrite(&map_byte, sizeof(uint8_t), 1, disk);
  // 修改组描述符
  // 如果存在多线程同时运行new_inode，下面需要互斥访问
  if (mod == INODE) {
    group_desc.bg_free_inodes_count--;
    // 计算索引结点号，填充索引结点
    last_alloc_inode = block_offset * 8 + byte_offset + 1;
    inode_t inode = load_inode_entry(last_alloc_inode);
    memset(&inode, 0, sizeof(inode_t));
    memset(&inode.i_pad, 0xff, 8);
    update_inode_entry(last_alloc_inode, &inode);
    return last_alloc_inode;
  } else if (mod == BLOCK) {
    group_desc.bg_free_blocks_count--;
    update_group_desc();
    last_alloc_block = block_offset * 8 + byte_offset;
    block_t block = load_block_entry(last_alloc_block);
    memset(&block, 0, sizeof(block_t));
    update_block_entry(last_alloc_block, &block);
    return last_alloc_block;
  } else {
    return -1;
  }
}

void free_inode_block(uint16_t No, alloc_mod mod) {
  uint8_t mask;
  if (mod == INODE) {
    mask = to_bitmap(group_desc.bg_inode_bitmap, No - 1);
  } else if (mod == BLOCK) {
    mask = to_bitmap(group_desc.bg_block_bitmap, No);
  } else {
    printf("free mod error!\n");
    return;
  }
  uint8_t map_byte;
  fread(&map_byte, sizeof(uint8_t), 1, disk); // 读出所在位图中的字节
  map_byte &= (~mask); // 其他位不变，inode_No所在位赋0
  // map_byte写回文件
  fseek(disk, -sizeof(uint8_t), SEEK_CUR);
  fwrite(&map_byte, sizeof(uint8_t), 1, disk);
  // 修改组描述符
  // 如果多线程同时运行，需要互斥访问
  if (mod == INODE) {
    group_desc.bg_free_inodes_count++;
  } else {
    group_desc.bg_free_blocks_count++;
  }
  update_group_desc();
}
