#include "disk.h"
#include "../logical_file_sys/logical_struct.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *disk = NULL; // 磁盘文件

// from block.c:
extern uint16_t last_alloc_block; // 上次分配的数据块号
extern uint16_t last_alloc_inode; // 上次分配的索引结点号

void format_disk() {
  FILE *disk_file = fopen(DISK_NAME, "wb+");
  // initialData占512Bytes, 即一个BLOCK的大小
  uint32_t *initial_data = (uint32_t *)calloc(128, sizeof(uint32_t));
  memset(initial_data, 0, 128);
  for (int itera = 0; itera < BLOCK_NUM; itera++) {
    fwrite(initial_data, sizeof(uint32_t), 128, disk_file);
  }
  free(initial_data);
  rewind(disk_file); // 文件指针回到开头
  disk = disk_file;
}

void to_group_desc() { rewind(disk); }

void to_inode(uint16_t inode_No, uint16_t block_offset) {
  rewind(disk);
  uint16_t offset = // inode_No减1是因为inode号从1计数
      BLOCK_SIZE * block_offset + (inode_No - 1) * sizeof(inode_t);
  fseek(disk, offset, SEEK_SET);
}

uint8_t to_bitmap(uint16_t block_offset, uint16_t no) {
  rewind(disk);
  uint16_t start_byte = BLOCK_SIZE * block_offset;
  uint16_t bytes = no / 8; // 相对位图起始位置的字节数
  uint16_t offset = no % 8;
  fseek(disk, start_byte + bytes, SEEK_SET);
  return 0x80 >> offset;
}

void to_block(uint16_t block_No) {
  rewind(disk);
  int offset = block_No + DATA_BLOCK_START;
  offset *= BLOCK_SIZE;
  fseek(disk, offset, SEEK_SET);
}

void to_passwd() {
  rewind(disk);
  fseek(disk, sizeof(group_desc_t), SEEK_CUR); // 密码在组描述符后
}
