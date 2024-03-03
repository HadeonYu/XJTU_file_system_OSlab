cmake_minimum_required(VERSION 3.5)
project(EXT2_SIM)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS "$ENV{CMAKE_CXX_FLAGS} -g -ggdb")

# set sources
set(FILE_ORGANIZATION_SOURCES 
  disk_manage/disk.c
)

set(LOGICAL_FILE_SYS_SOURCES 
  logical_file_sys/group.c 
  logical_file_sys/block.c
)

set(USER_INTERFACE_SOURCE 
  user_interface/file.c 
  user_interface/shell.c
)

set(SOURCES 
  main.c 
  ${FILE_ORGANIZATION_SOURCES}
  ${LOGICAL_FILE_SYS_SOURCES}
  ${USER_INTERFACE_SOURCE}
)

add_executable(${PROJECT_NAME} ${SOURCES})