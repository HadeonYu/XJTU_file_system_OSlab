#include "group.h"
#include "block.h"
#include "logical_struct.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

group_desc_t group_desc; // 组描述符

// from disk.c:
extern FILE *disk; // 磁盘文件指针
// from block.c:
extern uint16_t last_alloc_block; // 上次分配的数据块号
extern uint16_t last_alloc_inode; // 上次分配的索引结点号

void format_memory() {
  // 组描述符本身占1个块
  // 修改卷名
  while (1) {
    printf("Input volume name (within 16 charactors): ");
    scanf("%s", group_desc.bg_volume_name);
    if (group_desc.bg_volume_name[15] != 0) {
      printf("volume name too long!");
      continue;
    } else {
      break;
    }
  }
  // 修改数据块位图和索引结点位图
  group_desc.bg_block_bitmap = 1;              // 数据块位图占1号块
  group_desc.bg_inode_bitmap = 2;              // 索引结点位图占2号块
  group_desc.bg_inode_table = 3;               // 索引结点表占3号块
  group_desc.bg_free_inodes_count = INODE_NUN; // 空闲索引结点个数
  group_desc.bg_free_blocks_count = DATA_BLOCK_NUM; // 空闲块的个数
  group_desc.bg_used_dirs_count = 1;   // 当前目录个数，根目录
  memset(&group_desc.bg_pad, 0xff, 4); // 填充
  format_disk();                       // 磁盘层面上格式化磁盘
  fwrite(&group_desc, sizeof(group_desc_t), 1, disk);
  last_alloc_block = 0; // 块号从0开始计数
  last_alloc_inode = 1; // 索引结点号从1开始计数
  uint16_t root_inode_no = alloc_inode_block(INODE); // 根目录索引结点号
  // 因为是第一次分配，root_inode_no一定是1
  inode_t root_inode = load_inode_entry(root_inode_no);
  root_inode.i_blocks_count = 1;
  root_inode.i_atime = time(NULL);
  root_inode.i_ctime = time(NULL);
  root_inode.i_mtime = time(NULL);
  root_inode.i_mode = 0x02 << 8; // 高8位为2，表示目录
  root_inode.i_mode |= 0x7;      // 低三位rwx权限
  root_inode.i_size = 0;
  // 创建目录项.和..
  dir_entry_t cur_dir, pre_dir;
  cur_dir.inode = root_inode_no;
  cur_dir.file_type = 2;
  strcpy(cur_dir.name, ".");
  cur_dir.name_len = strlen(cur_dir.name);
  cur_dir.rec_len = DIR_ENTRY_BASIC_SIZE + cur_dir.name_len +
                    1; // 加1是因为需要包括文件名字符串末尾的'\0'
  root_inode.i_size += cur_dir.rec_len;
  pre_dir.inode = root_inode_no;
  pre_dir.file_type = 2;
  strcpy(pre_dir.name, "..");
  pre_dir.name_len = strlen(pre_dir.name);
  pre_dir.rec_len = DIR_ENTRY_BASIC_SIZE + pre_dir.name_len + 1;
  root_inode.i_size += pre_dir.rec_len;
  // 为根目录分配数据块
  uint16_t root_block_no = alloc_inode_block(BLOCK);
  root_inode.i_block[0] = root_block_no;
  block_t root_block = load_block_entry(root_block_no);
  // 目录项写入数据块
  memcpy(root_block.data, &cur_dir, cur_dir.rec_len);
  memcpy(root_block.data + cur_dir.rec_len, &pre_dir, pre_dir.rec_len);
  // 索引结点和数据块写入磁盘
  update_inode_entry(root_inode_no, &root_inode);
  update_block_entry(root_block_no, &root_block);
  fclose(disk);
  fopen(DISK_NAME, "rb+");
  // 更新密码
  char *input_passwd = (char *)calloc(256, sizeof(char));
  printf("Input password(within 255 of length): ");
  while (1) {
    fgets(input_passwd, 256, stdin);
    if (input_passwd[0] == '\n') {
      continue;
    } else {
      break;
    }
  }
  if (input_passwd[255] != 0 && input_passwd[255] != '\n') {
    printf("password too long\n\n");
    return;
  }
  uint8_t passwd_len = strlen(input_passwd); // 密码长度
  to_passwd();
  // 写入密码长度
  fwrite(&passwd_len, sizeof(uint8_t), 1, disk);
  // 写入密码内容
  fwrite(input_passwd, sizeof(char), passwd_len, disk);
  free(input_passwd);
}

void initial_memory() {
  // 读取组描述符
  disk = fopen(DISK_NAME, "rb+");
  if (!disk) {
    // 文件不存在，格式化
    printf("in initial, open file failed\n");
    format_memory();
    // 重新打开文件
    disk = fopen(DISK_NAME, "rb+");
  }
  // 读取组描述符
  rewind(disk);
  fread(&group_desc, sizeof(group_desc_t), 1, disk);
  if (!group_desc.bg_pad[0]) {
    // 末尾的填充位为0说明没格式化
    fclose(disk);
    format_memory();
  }
  fopen(DISK_NAME, "rb+");
  last_alloc_block = 0; // 块号从0开始计数
  last_alloc_inode = 1; // 索引结点号从1开始计数
}

void update_group_desc() {
  to_group_desc();
  fwrite(&group_desc, sizeof(group_desc_t), 1, disk);
}

void reload_group_desc() {
  to_group_desc();
  fread(&group_desc, sizeof(group_desc_t), 1, disk);
}

block_t *read_blocks(inode_t *inode) {
  block_t *blocks = (block_t *)calloc(inode->i_blocks_count, sizeof(block_t));
  uint16_t cur_count = 0;
  // 下面不用二级->一级->直接，是为了确保二级索引的最后一块在*blocks的末尾
  // 从而方便加入删除块
  // 直接索引
  if (!(inode->i_block[6] || inode->i_block[7])) {
    while (cur_count < inode->i_blocks_count) {
      load_block_muti_level(&blocks, &cur_count, inode->i_blocks_count,
                            inode->i_block[cur_count], 0);
    }
  }
  if (inode->i_block[7]) {
    // 二级索引
    while (cur_count < 6) {
      load_block_muti_level(&blocks, &cur_count, inode->i_blocks_count,
                            inode->i_block[cur_count], 0);
    }
    load_block_muti_level(&blocks, &cur_count, inode->i_blocks_count,
                          inode->i_block[6], 1);
    load_block_muti_level(&blocks, &cur_count, inode->i_blocks_count,
                          inode->i_block[7], 2);
  }
  if (inode->i_block[6]) {
    // 一级索引
    while (cur_count < 6) {
      load_block_muti_level(&blocks, &cur_count, inode->i_blocks_count,
                            inode->i_block[cur_count], 0);
    }
    load_block_muti_level(&blocks, &cur_count, inode->i_blocks_count,
                          inode->i_block[6], 1);
  }
  return blocks;
}

void update_blocks(inode_t *inode, block_t **blocks) {
  uint16_t cur_count = 0;
  if (!(inode->i_block[6] || inode->i_block[7])) {
    // 直接索引
    while (cur_count < inode->i_blocks_count) {
      update_block_muti_level(blocks, &cur_count, inode->i_blocks_count,
                              inode->i_block[cur_count], 0);
    }
  } else if (inode->i_block[7]) {
    // 二级索引
    while (cur_count < 6) {
      update_block_muti_level(blocks, &cur_count, inode->i_blocks_count,
                              inode->i_block[cur_count], 0);
    }
    update_block_muti_level(blocks, &cur_count, inode->i_blocks_count,
                            inode->i_block[6], 1);
    update_block_muti_level(blocks, &cur_count, inode->i_blocks_count,
                            inode->i_block[7], 2);
  } else if (inode->i_block[6]) {
    // 一级索引
    while (cur_count < 6) {
      update_block_muti_level(blocks, &cur_count, inode->i_blocks_count,
                              inode->i_block[cur_count], 0);
    }
    update_block_muti_level(blocks, &cur_count, inode->i_blocks_count,
                            inode->i_block[6], 1);
  }
}

void add_blocks(inode_t *inode, uint16_t **blocks_No,
                uint16_t new_blocks_count) {
  if (inode->i_blocks_count + new_blocks_count > 65798) { // 6+256+256*256
    printf("file size out of range\n");
    return;
  }
  int itera = 0;
  while (itera < new_blocks_count) {
    if (inode->i_blocks_count < 6) {
      // 直接索引
      inode->i_block[inode->i_blocks_count] = (*blocks_No)[itera];
      inode->i_blocks_count++;
      itera++;
    } else if (inode->i_blocks_count < 262) { // 6+256
      // 一级索引
      if (inode->i_block[6] == 0) {
        inode->i_block[6] = alloc_inode_block(BLOCK);
      }
      block_t sub_block = load_block_entry(inode->i_block[6]);
      int sub_index = (inode->i_blocks_count - 6) * 2;
      sub_block.data[sub_index] = (*blocks_No)[itera] >> 8;
      sub_block.data[sub_index + 1] = (*blocks_No)[itera] & 0xff;
      update_block_entry(inode->i_block[6], &sub_block);
      inode->i_blocks_count++;
      itera++;
    } else {
      // 二级索引
      if (inode->i_block[7] == 0) {
        inode->i_block[7] = alloc_inode_block(BLOCK);
        inode->i_blocks_count++;
      }
      // sub_index = i, sub2_index = j,
      // No = (256*i + j)
      block_t sub_block = load_block_entry(inode->i_block[7]);
      int sub_index = (inode->i_blocks_count - 262) / 256;
      sub_index *= 2;
      uint16_t sub2_block_No = sub_block.data[sub_index] << 8;
      sub2_block_No |= sub_block.data[sub_index + 1];
      if (sub2_block_No == 0) {
        sub2_block_No = alloc_inode_block(BLOCK);
        sub_block.data[sub_index] = sub2_block_No >> 8;
        sub_block.data[sub_index + 1] = sub2_block_No & 0xff;
        update_block_entry(inode->i_block[7], &sub_block);
      }
      block_t sub2_block = load_block_entry(sub2_block_No);
      int sub2_index = (inode->i_blocks_count - 263) % 256;
      sub2_index *= 2;
      sub_block.data[sub2_index] = (*blocks_No)[itera] >> 8;
      sub_block.data[sub2_index + 1] = (*blocks_No)[itera] & 0xff;
      update_block_entry(sub2_block_No, &sub2_block);
      inode->i_blocks_count++;
      itera++;
    }
  }
}

void free_blocks(inode_t *inode) {
  for (int itera = 0; itera < 6; itera++) {
    if (inode->i_block[itera] == 0) {
      break;
    }
    free_inode_block(inode->i_block[itera], BLOCK);
  }
  if (inode->i_block[6]) {
    // 一级索引
    block_t sub_block = load_block_entry(inode->i_block[6]);
    for (int itera = 0; itera < BLOCK_SIZE; itera += 2) {
      if (sub_block.data[itera] == 0) {
        break;
      }
      uint16_t remove_block_No = sub_block.data[itera] << 8;
      remove_block_No |= sub_block.data[itera + 1];
      free_inode_block(remove_block_No, BLOCK);
    }
  }
  if (inode->i_block[7]) {
    // 二级索引
    block_t sub_block = load_block_entry(inode->i_block[7]);
    for (int sub_index = 0; sub_index < BLOCK_SIZE; sub_index += 2) {
      if (sub_block.data[sub_index] == 0) {
        break;
      }
      uint16_t sub2_index = sub_block.data[sub_index] << 8;
      sub2_index |= sub_block.data[sub_index + 1];
      block_t sub2_block = load_block_entry(sub2_index);
      for (int itera = 0; itera < BLOCK_SIZE; itera += 2) {
        if (sub2_block.data[itera] == 0) {
          break;
        }
        uint16_t remove_block_No = sub2_block.data[itera] << 8;
        remove_block_No |= sub2_block.data[itera + 1];
        free_inode_block(remove_block_No, BLOCK);
      }
    }
  }
  inode->i_blocks_count = 0;
}

dir_info_t load_dir_entry(uint16_t inode_No) {
  inode_t inode = load_inode_entry(inode_No);
  block_t *blocks = read_blocks(&inode);
  dir_entry_t *dir = (dir_entry_t *)calloc(inode.i_size, 1); // 目录项数组
  memcpy(dir, blocks, inode.i_size);
  free(blocks);
  uint16_t dir_count = 0; // 目录项的数量
  int cur_len = 0;        // 当前遍历的目录项的总长度
  dir_entry_t *dir_itera = dir;
  while (cur_len != inode.i_size) {
    cur_len += dir_itera->rec_len;
    if (dir_itera->rec_len == 0) {
      printf("reach invalid directory entry!\n\n");
      dir_info_t ret = {0, 0};
      return ret;
    }
    dir_count++;
    move_dir_entry(&dir_itera);
  }
  dir_info_t ret = {dir, dir_count};
  return ret;
}

dir_entry_t create_dir_entry(const char *name, uint16_t inode_No,
                             uint8_t file_type) {
  dir_entry_t dir;
  memset(&dir, 0, sizeof(dir_entry_t));
  strcpy(dir.name, name);
  dir.name_len = strlen(name);
  dir.rec_len = DIR_ENTRY_BASIC_SIZE + dir.name_len + 1;
  dir.inode = inode_No;
  dir.file_type = file_type;
  return dir;
}

void update_dir_entry(uint16_t inode_No, dir_entry_t *dirs) {
  inode_t inode = load_inode_entry(inode_No);
  block_t *blocks = read_blocks(&inode);
  memcpy(blocks, dirs, inode.i_size);
  update_blocks(&inode, &blocks);
}

void move_dir_entry(dir_entry_t **dir) {
  *dir = (dir_entry_t *)((char *)(*dir) + (*dir)->rec_len);
}
