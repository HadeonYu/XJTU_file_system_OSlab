#ifndef FILE_H
#define FILE_H

#include "../logical_file_sys/block.h"
#include "../logical_file_sys/group.h"
#include <stdint.h>

#define MAX_PATH_DEPTH 16 // 所在的目录深度最大为64

// 当前所在的目录结构体
typedef struct current_path_t {
  uint16_t path_inode[MAX_PATH_DEPTH]; // 路径数组，内容是inode号，栈
  uint8_t depth;       // 路径深度，也就是path数组的长度
  char path_name[256]; // 路径名，从根目录到当前目录
  dir_entry_t *dirs;   // 当前目录中的目录项数组
  uint16_t dir_count;  // 目录项个个数
} current_path_t;

// 文件属性
typedef struct file_attr {
  char mode[5];         // 类型和权限
  char create_time[50]; // 创建时间
  char modify_time[50]; // 修改时间
  char access_time[50]; // 访问时间
  int size;             // 文件大小
  char name[256];       // 文件名
} file_attr;

// 初始化，打开根目录
void init();

// 获取目录项的属性，若inode为0则文件名字段为NULL
file_attr rec_attr(dir_entry_t *dir);

// 根据文件名返回索引结点号
uint16_t open(const char *name);

// 创建文件
void create(int argc, char **argv);

void _delete(int argc, char **argv);

void read(int argc, char **argv);

void write(int argc, char **argv);

void mkdir(int argc, char **argv);

void rmdir(int argc, char **argv);

#endif // !FILE_H
