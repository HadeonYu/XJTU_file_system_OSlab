#include "shell.h"
#include "file.h"
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

/*
 * 哈希表，长度为26
 * 命令的首字母减 'a' 得到命令在表中的下标
 * 若首字母重复则用next指针
 */
command_map *hash_list;

// from disk.c:
extern FILE *disk;

// from file.c:
extern uint16_t fopen_table[16]; // 文件打开表，最多同时打开16文件
extern current_path_t cur_path;  // 当前路径

// from group.c:
extern group_desc_t group_desc; // 组描述符

void shell() {
  init();
  system("clear");
  login();
  printf("welcome\n");
  printf("type \"help\" to list all commands and usage\n");
  printf("type \"help [command]\" to list certain usage\n\n");
  init_hash_list(); // 初始化命令哈希表
  // 循环输入命令
  char user_input[256];
  while (1) {
    printf("root@%s:%s # ", group_desc.bg_volume_name, cur_path.path_name);
    fgets(user_input, 255, stdin);
    if (user_input[0] < 'a' || user_input[0] > 'z') {
      continue;
    }
    user_input[strlen(user_input) - 1] = 0; // 换行符改为结束
    // 解析并执行命令
    parse_command(user_input);
  }
}

// 释放给argv分配到空间
void free_argv(int argc, char **argv) {
  for (int itera = 0; itera < argc; itera++) {
    free(argv[itera]);
  }
  free(argv);
}

void parse_command(const char *user_input) {
  char command[strlen(user_input)]; // 输入的指令部分
  int itera = 0;
  for (; user_input[itera] != ' ' && user_input[itera] != 0; itera++) {
    command[itera] = user_input[itera];
  }
  command[itera] = 0;
  itera++;
  int arg_len = strlen(user_input) - itera; // 用户输入中参数部分的长度
  int argc; // 参数的个数，也就是参数数组的长度
  char **argv = (char **)calloc(16, sizeof(char *)); // 参数数组
  if (arg_len > 0) {
    argc = 1;
    char **argv_itera = argv;
    int single_arg_index = 0; // 一个参数字符串的下标
    *argv_itera = (char *)calloc(256, sizeof(char));
    for (; user_input[itera] != 0; itera++) {
      if (user_input[itera] == ' ') {
        argc++;
        (*argv_itera)[single_arg_index] = 0;
        argv_itera++;
        *argv_itera = (char *)calloc(256, sizeof(char));
        memset(*argv_itera, 0, 256 * sizeof(char));
        single_arg_index = 0;
        continue;
      }
      (*argv_itera)[single_arg_index] = user_input[itera];
      single_arg_index++;
    }
  } else {
    argc = 0;
  }
  command_func func = get_func(command);
  if (!func) {
    printf("\ncommand not found: %s\n\n", command);
    return;
  }
  func(argc, argv);
}

void init_hash_list() {
  hash_list = (command_map *)calloc(26, sizeof(command_map));
  memset(hash_list, 0, 26 * sizeof(command_map));
  const command_map command_list[20] = {
      {"ls", ls, NULL},           {"cd", cd, NULL},
      {"create", create, NULL},   {"delete", _delete, NULL},
      {"read", read, NULL},       {"write", write, NULL},
      {"format", format, NULL},   {"password", password, NULL},
      {"mkdir", mkdir, NULL},     {"rmdir", rmdir, NULL},
      {"quit", shutdown, NULL},   {"clear", clear, NULL},
      {"help", print_help, NULL},
  };
  int command_count = 13;
  for (int itera = 0; itera < command_count; itera++) {
    int hash_index = command_list[itera].command_name[0] - 'a';
    command_map *hash_map = &hash_list[hash_index];
    while (hash_map->command_name) {
      if (hash_map->next) {
        hash_map = hash_map->next;
      } else {
        hash_map->next = (command_map *)malloc(sizeof(command_map));
        memset(hash_map->next, 0, sizeof(command_map));
        hash_map = hash_map->next;
      }
    }
    hash_map->command_name =
        (char *)calloc(strlen(command_list[itera].command_name), sizeof(char));
    strcpy(hash_map->command_name, command_list[itera].command_name);
    hash_map->func = command_list[itera].func;
  }
}

command_func get_func(const char *command) {
  command_map map = hash_list[command[0] - 'a'];
  if (!map.command_name) {
    return NULL; // hash结果找不到命令
  }
  while (strcmp(map.command_name, command)) {
    if (!map.next) {
      return NULL; // 找不到命令
    }
    map = *map.next;
  }
  return map.func;
}

void print_help(int argc, char **argv) {
  char *command_name = argv[0];
  printf("\n");
  if (!argc || !strcmp(command_name, "help")) {
    printf("help: print help information\n");
    printf("  usage: help [command name]\n\n");
  }
  if (!argc || !strcmp(command_name, "ls")) {
    printf("ls: list all directory entry in current directory\n\n");
  }
  if (!argc || !strcmp(command_name, "cd")) {
    printf("cd: change current directory\n");
    printf("  usage: cd [directory name]\n\n");
  }
  if (!argc || !strcmp(command_name, "create")) {
    printf("create: create a file in current directory\n");
    printf("  usage: create [file name]\n\n");
  }
  if (!argc || !strcmp(command_name, "delete")) {
    printf("delete: delete a file in current directory\n");
    printf("  usage: delete [file name]\n\n");
  }
  if (!argc || !strcmp(command_name, "read")) {
    printf("read: read a file in current directory\n");
    printf("  usage: read [file name]\n\n");
  }
  if (!argc || !strcmp(command_name, "write")) {
    printf("write: write a file in current directory\n");
    printf("  usage: write -n [file name]\n");
    printf("               -f [real file from disk]\n\n");
  }
  if (!argc || !strcmp(command_name, "format")) {
    printf("format: format the disk\n\n");
  }
  if (!argc || !strcmp(command_name, "password")) {
    printf("password: change the password\n\n");
  }
  if (!argc || !strcmp(command_name, "mkdir")) {
    printf("mkdir: create a directory in current directory\n");
    printf("  usage: mkdir [directory name]\n\n");
  }
  if (!argc || !strcmp(command_name, "rmdir")) {
    printf("rmdir: delete a directory in current directory\n");
    printf("  usage: rmdir [directory name]\n\n");
  }
  if (!argc || !strcmp(command_name, "quit")) {
    printf("quit: quit the system\n\n");
  }
  if (!argc || !strcmp(command_name, "clear")) {
    printf("clear: clear the screen\n\n");
  }
  free_argv(argc, argv);
}

// 获取密码
char *get_passwd() {
  to_passwd();
  uint8_t passwd_len;
  // 前一字节是密码长度
  fread(&passwd_len, sizeof(uint8_t), 1, disk);
  char *passwd = (char *)calloc(passwd_len, sizeof(char));
  fread(passwd, sizeof(char), passwd_len, disk);
  return passwd;
}

void login() {
  printf("type \"quit\" to exit\n");
  printf("Input password: ");
  char *cur_passwd = get_passwd(); // 获取保存的密码
  char *input_passwd = (char *)calloc(256, sizeof(char));
  while (1) {
    fgets(input_passwd, 256, stdin);
    if (!strcmp(input_passwd, "quit\n")) {
      exit(0);
    }
    // 输入的和保存的比对
    if (!strcmp(input_passwd, cur_passwd)) {
      break;
    } else {
      printf("incorrect password!\n\n");
      printf("type \"quit\" to exit\n");
    }
  }
  free(cur_passwd);
  free(input_passwd);
  return;
}

void password(int argc, char **argv) {
  if (argc != 0) {
    printf("unknown arguments: ");
    for (int i = 0; i < argc; i++) {
      printf("%s\t", argv[i]);
    }
    printf("\n\n");
    return;
  }
  char *input_passwd = (char *)calloc(256, sizeof(char));
  // 旧密码输入正确后允许改密码
  printf("Input old password: ");
  fgets(input_passwd, 256, stdin);
  char *cur_passwd = get_passwd();
  if (strcmp(input_passwd, cur_passwd)) {
    free(cur_passwd);
    printf("incorrect password!\n\n");
    return;
  }
  free(cur_passwd);
  printf("Input new password(within 255 of length): ");
  fgets(input_passwd, 256, stdin);
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
  free_argv(argc, argv);
}

void ls(int argc, char **argv) {
  dir_entry_t *dirs = cur_path.dirs;
  printf("mode\tsize\tcreate\t\tmodify\t\taccess\t\tname\n");
  for (int itera = 0; itera < cur_path.dir_count; itera++) {
    file_attr attr = rec_attr(dirs);
    if (!attr.name[0]) {
      continue; // 已被删除
    }
    printf("%s\t%d\t%s\t%s\t%s\t%s\n", attr.mode, attr.size, attr.create_time,
           attr.modify_time, attr.access_time, attr.name);
    move_dir_entry(&dirs);
  }
  printf("\n");
  free_argv(argc, argv);
}

void cd(int argc, char **argv) {
  if (argc == 0) {
    printf("missing argument: directory name\n\n");
    return;
  }
  const char *dir_name = argv[0];
  if (!strcmp(dir_name, ".")) { // cd到当前目录
    return;
  }
  if (!strcmp(dir_name, "..") && cur_path.depth == 1) {
    // 当前在根目录
    return;
  }
  // 找到要进入的目录
  if (cur_path.depth == MAX_PATH_DEPTH) {
    printf("reach the max of path depth\n\n");
    return;
  }
  dir_entry_t *dirs_itera = cur_path.dirs;
  uint16_t dir_inode_No = 0;
  for (int itera = 0; itera < cur_path.dir_count; itera++) {
    if (!strcmp(dir_name, dirs_itera->name)) {
      dir_inode_No = dirs_itera->inode;
      break;
    }
    move_dir_entry(&dirs_itera);
  }
  if (dirs_itera->file_type != DIR) {
    printf("%s is not a directory\n\n", dir_name);
    return;
  }
  // 更新cur_path
  if (!strcmp(dir_name, "..")) {
    // 返回上级目录
    cur_path.depth--;
    char *path_name_itera = cur_path.path_name + strlen(cur_path.path_name);
    while (*path_name_itera != '/') {
      *path_name_itera = 0;
      path_name_itera--;
    }
    *path_name_itera = 0;
  } else {
    cur_path.path_inode[cur_path.depth] = dir_inode_No;
    cur_path.depth++;
    strcat(cur_path.path_name, dirs_itera->name);
  }
  inode_t dir_inode = load_inode_entry(dir_inode_No);
  dir_info_t dir_info = load_dir_entry(dir_inode_No);
  free(cur_path.dirs);
  cur_path.dirs = dir_info.dir;
  cur_path.dir_count = dir_info.dir_count;
  free_argv(argc, argv);
}

void format(int argc, char **argv) {
  if (argc != 0) {
    printf("unknown arguments: ");
    for (int i = 0; i < argc; i++) {
      printf("%s\t", argv[i]);
    }
    printf("\n\n");
    return;
  }
  fclose(disk);
  format_memory();
  init();
  free_argv(argc, argv);
}

void shutdown(int argc, char **argv) {
  if (argc) {
    for (int i = 0; i < argc; i++) {
      printf("unknown arguments: ");
      printf("%s\t", argv[i]);
    }
    printf("\n\n");
    return;
  }
  free(hash_list);
  free(disk);
  update_group_desc();
  exit(0);
}

void clear(int argc, char **argv) {
  if (argc > 0) {
    printf("unknown argument: ");
    for (int i = 0; i < argc; i++) {
      printf("%s ", *(argv + i));
    }
    printf("\n\n");
    return;
  }
  system("clear");
}
