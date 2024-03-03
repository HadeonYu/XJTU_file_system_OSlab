#ifndef SHELL_H
#define SHELL_H

#include "../logical_file_sys/block.h"
#include "../logical_file_sys/group.h"
#include <stdint.h>

// 执行命令的函数，参数为命令的参数数量和参数数组
typedef void (*command_func)(int, char **);

// 命令到函数的映射结构，next指针在哈希表中用到
typedef struct command_map {
  char *command_name;
  command_func func;
  struct command_map *next;
} command_map;

// 启动用户接口
void shell();

// 从输入中解析出命令和参数，并执行对应的函数
void parse_command(const char *user_input);

// 初始化哈希表
void init_hash_list();

// 通过哈希表，返回输入的命令对应的函数
command_func get_func(const char *command);

// 输出各个命令的用法
void print_help(int argc, char **argv);

// 释放给argv分配到空间
void free_argv(int argc, char **argv);

// 启动程序时运行，登录root(只设计了一个用户)
void login();

// 改密码
void password(int argc, char **argv);

// 列出当前目录的所有目录项
void ls(int argc, char **argv);

// 进入目录，只实现了进入当前目录下的目录
void cd(int argc, char **argv);

// 格式化
void format(int argc, char **argv);

// 退出
void shutdown(int argc, char **argv);

void clear(int argc, char **argv);

#endif // !SHELL_H
