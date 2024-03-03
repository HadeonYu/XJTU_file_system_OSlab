#include "file.h"
#include "shell.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

current_path_t cur_path; // 当前路径

// from group.c:
extern group_desc_t group_desc; // 组描述符

void init() {
  initial_memory(); // 调用group.c的initial_memory，初始化内存
  // 初始化当前路径
  cur_path.path_inode[0] = 1; // 根目录inode结点号为1
  cur_path.depth = 1;
  inode_t inode = load_inode_entry(1);
  block_t block = load_block_entry(0);
  dir_entry_t *dir = (dir_entry_t *)calloc(inode.i_size, 1); // 目录项数组
  memcpy(dir, &block, inode.i_size);
  uint16_t dir_count = 0; // 目录项的数量
  int cur_len = 0;        // 当前遍历的目录项的总长度
  dir_entry_t *dir_itera = dir;
  while (cur_len != inode.i_size) {
    cur_len += dir_itera->rec_len;
    dir_count++;
    move_dir_entry(&dir_itera);
  }
  cur_path.dirs = dir;
  cur_path.dir_count = dir_count;
  strcpy(cur_path.path_name, "/");
}

file_attr rec_attr(dir_entry_t *dir) {
  inode_t inode = load_inode_entry(dir->inode); // 加载inode
  file_attr ret;

  // 文件名
  if (dir->inode != 0) {
    strcpy(ret.name, dir->name);
  } else {
    ret.name[0] = 0;
  }

  // 文件大小
  if (dir->file_type == DIR) {
    ret.size = dir->rec_len;
  } else {
    inode_t inode = load_inode_entry(dir->inode);
    ret.size = inode.i_size;
  }

  // 访问时间
  time_t rawtime = inode.i_atime;
  struct tm *ptm = localtime(&rawtime);
  char access_time[50];
  strftime(access_time, 50, "%m-%d %H:%M", ptm);
  strcpy(ret.access_time, access_time);

  // 修改时间
  rawtime = inode.i_mtime;
  ptm = localtime(&rawtime);
  char modify_time[50];
  strftime(modify_time, 50, "%m-%d %H:%M", ptm);
  strcpy(ret.modify_time, modify_time);

  // 创建时间
  rawtime = inode.i_ctime;
  ptm = localtime(&rawtime);
  char create_time[50];
  strftime(create_time, 50, "%m-%d %H:%M", ptm);
  strcpy(ret.create_time, create_time);

  // 类型和权限
  char mode[5] = {'-', '-', '-', '-', 0};
  uint16_t type = dir->file_type;
  if (type == 2) {
    mode[0] = 'd';
  }
  // i_mode 低三位表示权限
  uint16_t mask = 0x0004; // 0000_0000_0000_0100b
  if (mask & inode.i_mode) {
    mode[1] = 'r';
  }
  mask >>= 1;
  if (mask & inode.i_mode) {
    mode[2] = 'w';
  }
  mask >>= 1;
  if (mask & inode.i_mode) {
    mode[3] = 'x';
  }
  strcpy(ret.mode, mode);
  return ret;
}

uint16_t open(const char *name) {
  dir_entry_t *cur_dirs = cur_path.dirs;
  uint16_t inode_No = 0;
  for (int itera = 0; itera < cur_path.dir_count; itera++) {
    if (!strcmp(name, cur_dirs->name)) {
      inode_No = cur_dirs->inode;
      break;
    }
    move_dir_entry(&cur_dirs);
  }
  return inode_No;
}

void create(int argc, char **argv) {
  if (argc == 0) {
    printf("missing argument: file name\n\n");
    return;
  }
  const char *name = argv[0];
  // 尝试打开文件，若返回不为0则重名
  uint16_t inode_No_tmp = open(name);
  if (inode_No_tmp) {
    printf("file %s already exits\n\n", name);
    return;
  }
  uint16_t cur_inode_No = cur_path.path_inode[cur_path.depth - 1];
  inode_t cur_inode = load_inode_entry(cur_inode_No);
  // 构造inode
  uint16_t create_inode_No = alloc_inode_block(INODE);
  inode_t create_inode = load_inode_entry(create_inode_No);
  create_inode.i_size = 0;
  create_inode.i_ctime = time(NULL);
  create_inode.i_atime = time(NULL);
  create_inode.i_mtime = time(NULL);
  create_inode.i_mode = 0x01 << 8;
  create_inode.i_mode |= 0x06;
  update_inode_entry(create_inode_No, &create_inode);
  // 构建目录项，加入当前目录
  dir_entry_t create_dir = create_dir_entry(name, create_inode_No, COMMON);
  dir_entry_t *cur_dirs = cur_path.dirs;
  dir_entry_t *new_dirs =
      (dir_entry_t *)malloc(cur_inode.i_size + create_dir.rec_len);
  memset(new_dirs, 0, cur_inode.i_size + create_dir.rec_len);
  memcpy(new_dirs, cur_dirs, cur_inode.i_size);
  free(cur_dirs);
  cur_path.dirs = new_dirs;
  for (int itera = 0; itera < cur_path.dir_count; itera++) {
    move_dir_entry(&new_dirs);
  }
  memcpy(new_dirs, &create_dir, create_dir.rec_len);
  new_dirs->name[strlen(name)] = 0;
  cur_path.dir_count++;
  // 更新inode和目录项
  cur_inode.i_size += create_dir.rec_len;
  cur_inode.i_atime = time(NULL);
  cur_inode.i_mtime = time(NULL);
  update_inode_entry(cur_inode_No, &cur_inode);
  update_dir_entry(cur_inode_No, cur_path.dirs);
  free_argv(argc, argv);
}

void _delete(int argc, char **argv) {
  if (argc == 0) {
    printf("missing argument: file name\n\n");
    return;
  }
  const char *name = argv[0];
  uint16_t cur_inode_No = cur_path.path_inode[cur_path.depth - 1];
  inode_t cur_inode = load_inode_entry(cur_inode_No);
  dir_entry_t *cur_dirs = cur_path.dirs;
  dir_entry_t *delete_dir = NULL;
  dir_entry_t *pre_dir; // 被删除的目录项的前一个目录项，需要修改rec_len字段
  // 找出要删除的目录项
  for (int i = 0; i < cur_path.dir_count; i++) {
    if (!strcmp(name, cur_dirs->name)) {
      delete_dir = cur_dirs;
      break;
    }
    pre_dir = cur_dirs;
    move_dir_entry(&cur_dirs);
  }
  if (!delete_dir) {
    printf("cannot find file: %s\n\n", name);
    return;
  }
  if (delete_dir->file_type != COMMON) {
    printf("%s is not a file\n\n", name);
    return;
  }
  pre_dir->rec_len += delete_dir->rec_len;
  memset(delete_dir, 0, delete_dir->rec_len);
  // 更新当前目录项
  update_dir_entry(cur_path.path_inode[cur_path.depth - 1], cur_path.dirs);
  // 释放占用的块
  free_blocks(&cur_inode);
  // 释放索引结点
  free_inode_block(cur_inode_No, INODE);
  free_argv(argc, argv);
}

void read(int argc, char **argv) {
  if (argc == 0) {
    printf("missing argument: file name\n\n");
    return;
  }
  const char *name = argv[0];
  dir_entry_t *cur_dirs = cur_path.dirs;
  dir_entry_t *read_dir = NULL;
  // 找出要读取的目录项
  for (int i = 0; i < cur_path.dir_count; i++) {
    if (!strcmp(name, cur_dirs->name)) {
      read_dir = cur_dirs;
      break;
    }
    move_dir_entry(&cur_dirs);
  }
  if (!read_dir) {
    printf("cannot find file: %s\n\n", name);
    return;
  }
  if (read_dir->file_type == DIR) {
    printf("cannot read a directory\n\n");
    return;
  }
  // 输出内容
  inode_t read_inode = load_inode_entry(read_dir->inode);
  uint16_t cur_count = 0;
  block_t *blocks = read_blocks(&read_inode);
  printf("%s\n", (char *)blocks);
  free(blocks);
  // 修改索引结点
  read_inode.i_atime = time(NULL);
  update_inode_entry(read_dir->inode, &read_inode);
  free_argv(argc, argv);
}

void write(int argc, char **argv) {
  char *name = NULL;
  FILE *file = NULL;
  if (argc == 0) {
    printf("missing all arguments\n\n");
    return;
  }
  for (int i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "-n")) {
      if (i + 1 >= argc) {
        printf("missing argument: file name\n\n");
        return;
      }
      name = argv[i + 1];
      i++;
    }
    if (!strcmp(argv[i], "-f")) {
      if (i + 1 >= argc) {
        printf("missing argument: opened file\n\n");
        return;
      }
      file = fopen(argv[i + 1], "rb");
      if (!file) {
        printf("cannot open file: %s\n\n", argv[i + 1]);
        return;
      }
      i++;
    }
  }
  if (!name) {
    printf("missing argument: file name\n\n");
    return;
  }
  if (!file) {
    file = stdin;
  }
  dir_entry_t *cur_dirs = cur_path.dirs;
  dir_entry_t *write_dir = NULL;
  // 找出要写入的目录项
  for (int i = 0; i < cur_path.dir_count; i++) {
    if (!strcmp(name, cur_dirs->name)) {
      write_dir = cur_dirs;
      break;
    }
    move_dir_entry(&cur_dirs);
  }
  if (!write_dir) {
    printf("cannot find file: %s\n\n", name);
    return;
  }
  if (write_dir->file_type == DIR) {
    printf("cannot write a directory\n\n");
    return;
  }
  // 清空文件原内容
  uint16_t write_inode_No = write_dir->inode;
  inode_t write_inode = load_inode_entry(write_inode_No);
  free_blocks(&write_inode);
  // 输入内容，分配数据块
  int blocks_count = 1; // 数据块的数量
  int total_size = 0;   // 输入的内容总字节数
  uint16_t *block_No =
      (uint16_t *)calloc(4096, sizeof(uint16_t)); // 分配的数据块号
  block_t *blocks = (block_t *)calloc(4096, sizeof(block_t)); // 数据块
  memset(blocks, 0, 4096 * sizeof(block_t));
  block_No[0] = alloc_inode_block(BLOCK);
  // 输入内容
  if (file == stdin) {
    printf("Input the content of file\n");
    printf("Input \"ijkl\" in a single line to finish\n");
  }
  while (1) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, file);
    if (read == -1) {
      break;
    }
    if (file == stdin && !strcmp(line, "ijkl\n")) {
      break;
    }
    total_size += strlen(line);
    if (total_size > BLOCK_SIZE * group_desc.bg_free_blocks_count) {
      printf("file size out of range\n");
      break;
    }
    if (total_size > blocks_count * BLOCK_SIZE) {
      block_No[blocks_count] = alloc_inode_block(BLOCK);
      blocks_count++;
    }
    strcat((char *)blocks, line);
  }
  // 写入数据块
  add_blocks(&write_inode, &block_No, blocks_count);
  update_blocks(&write_inode, &blocks);
  // 更新索引结点
  write_inode.i_atime = time(NULL);
  write_inode.i_mtime = time(NULL);
  write_inode.i_size = total_size;
  update_inode_entry(write_inode_No, &write_inode);
  free(block_No);
  free(blocks);
  free_argv(argc, argv);
}

void mkdir(int argc, char **argv) {
  if (argc == 0) {
    printf("missing argument: directory name\n\n");
    return;
  }
  char *dir_name = argv[0];
  if (strlen(dir_name) > FILE_NAME_LEN) {
    printf("directory name length out of range\n\n");
    return;
  }
  if (open(dir_name) != 0) {
    printf("%s already exits\n\n", dir_name);
    return;
  }
  uint16_t cur_inode_No =
      cur_path.path_inode[cur_path.depth - 1]; // 当前目录的索引结点号
  inode_t inode = load_inode_entry(cur_inode_No); // 当前目录的索引结点
  // 构造目录项
  uint16_t dir_inode_No = alloc_inode_block(INODE);
  dir_entry_t new_dir = create_dir_entry(dir_name, dir_inode_No, DIR);
  // 更新current_path
  dir_entry_t *new_dir_list =
      (dir_entry_t *)calloc(inode.i_size + new_dir.rec_len, 1);
  memcpy(new_dir_list, cur_path.dirs, inode.i_size);
  dir_entry_t *new_dir_itera = new_dir_list;
  for (int i = 0; i < cur_path.dir_count; i++) {
    move_dir_entry(&new_dir_itera);
  }
  memcpy(new_dir_itera, &new_dir, new_dir.rec_len);
  free(cur_path.dirs);
  cur_path.dirs = new_dir_list;
  cur_path.dir_count++;
  // 更新目录项的索引结点
  inode_t dir_inode = load_inode_entry(dir_inode_No);
  dir_inode.i_blocks_count = 1;
  dir_inode.i_atime = time(NULL);
  dir_inode.i_ctime = time(NULL);
  dir_inode.i_mtime = time(NULL);
  dir_inode.i_mode = 0x02 << 8; // 高8位为2，表示目录
  dir_inode.i_mode |= 0x7;      // 低三位rwx权限
  dir_inode.i_size = 0;
  // 创建目录项.和..
  dir_entry_t cur_dir, pre_dir;
  cur_dir.inode = dir_inode_No;
  cur_dir.file_type = DIR;
  strcpy(cur_dir.name, ".");
  cur_dir.name_len = strlen(cur_dir.name);
  cur_dir.rec_len = DIR_ENTRY_BASIC_SIZE + cur_dir.name_len +
                    1; // 加1是因为需要包括文件名字符串末尾的'\0'
  dir_inode.i_size += cur_dir.rec_len;
  pre_dir.inode = cur_inode_No;
  pre_dir.file_type = DIR;
  strcpy(pre_dir.name, "..");
  pre_dir.name_len = strlen(pre_dir.name);
  pre_dir.rec_len = DIR_ENTRY_BASIC_SIZE + pre_dir.name_len + 1;
  dir_inode.i_size += pre_dir.rec_len;
  // 创建数据块
  uint16_t dir_block_No = alloc_inode_block(BLOCK);
  dir_inode.i_block[0] = dir_block_No;
  block_t dir_block = load_block_entry(dir_block_No);
  // 目录项写入数据块
  memcpy(dir_block.data, &cur_dir, cur_dir.rec_len);
  memcpy(dir_block.data + cur_dir.rec_len, &pre_dir, pre_dir.rec_len);
  // 新目录索引结点和数据块写入磁盘
  update_inode_entry(dir_inode_No, &dir_inode);
  update_block_entry(dir_block_No, &dir_block);
  // 更新当前目录的索引结点和数据块
  block_t *blocks = read_blocks(&inode);
  if (inode.i_size + new_dir.rec_len > inode.i_blocks_count * BLOCK_SIZE) {
    // 需要分配块
    // 由于对文件名大小的限制，一次只需分配一个块
    uint16_t new_block_No = alloc_inode_block(BLOCK);
    // new_block_No加入inode->i_block
    uint16_t *No_list = (uint16_t *)malloc(sizeof(uint16_t));
    No_list[0] = new_block_No;
    uint16_t old_blocks_count =
        inode.i_blocks_count; // add_blocks函数会改变这一字段
    add_blocks(&inode, &No_list, 1);
    block_t *new_blocks = (block_t *)calloc(old_blocks_count, sizeof(block_t));
    memcpy(new_blocks, blocks, old_blocks_count * sizeof(block_t));
    new_blocks[old_blocks_count - 1] = load_block_entry(new_block_No);
    free(blocks);
    blocks = new_blocks;
  }
  memcpy(blocks, new_dir_list, inode.i_blocks_count * sizeof(block_t));
  inode.i_size += new_dir.rec_len;
  inode.i_atime = time(NULL);
  inode.i_mtime = time(NULL);
  update_inode_entry(cur_inode_No, &inode);
  update_blocks(&inode, &blocks);
  // 更新组描述符
  group_desc.bg_used_dirs_count++;
  update_group_desc();
  free_argv(argc, argv);
}

void rmdir(int argc, char **argv) {
  if (argc == 0) {
    printf("missing argument: directory name\n\n");
    return;
  }
  char *name = argv[0];
  if (!strcmp(name, ".") || !strcmp(name, "..")) {
    printf("cannot remove directory: %s\n\n", name);
    return;
  }
  dir_entry_t *cur_dirs = cur_path.dirs;
  dir_entry_t *remove_dir = NULL;
  dir_entry_t *pre_dir; // 被删除的目录项的前一个目录项，需要修改rec_len字段
  // 找出要删除的目录项
  for (int i = 0; i < cur_path.dir_count; i++) {
    if (!strcmp(name, cur_dirs->name)) {
      remove_dir = cur_dirs;
      break;
    }
    pre_dir = cur_dirs;
    move_dir_entry(&cur_dirs);
  }
  if (!remove_dir) {
    printf("cannot find directory: %s\n\n", name);
    return;
  }
  if (remove_dir->file_type != DIR) {
    printf("%s is not a directory\n\n", name);
    return;
  }
  pre_dir->rec_len += remove_dir->rec_len;
  memset(remove_dir, 0, remove_dir->rec_len);
  // 更新目录项
  update_dir_entry(cur_path.path_inode[cur_path.depth - 1], cur_path.dirs);
  // 删除文件夹内容
  inode_t remove_inode = load_inode_entry(remove_dir->inode);
  dir_info_t remove_dir_info = load_dir_entry(remove_dir->inode);
  move_dir_entry(&remove_dir_info.dir);
  move_dir_entry(&remove_dir_info.dir); // 跳过.和..
  for (int i = 0; i < remove_dir_info.dir_count; i++) {
    char *delete_name = remove_dir_info.dir->name;
    if (delete_name[0] == 0) {
      break;
    }
    if (remove_dir_info.dir->file_type == DIR) {
      // 递归删除
      rmdir(1, &delete_name);
    } else {
      _delete(1, &delete_name);
    }
    move_dir_entry(&remove_dir_info.dir);
  }
  // 释放数据块
  free_blocks(&remove_inode);
  // 更新组描述符
  group_desc.bg_used_dirs_count--;
  update_group_desc();
  free_argv(argc, argv);
}
