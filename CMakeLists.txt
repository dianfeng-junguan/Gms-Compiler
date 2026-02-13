cmake_minimum_required(VERSION 3.10)
project(GMS_COMPILER)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_COMPILER gcc)
add_executable(GMS_COMPILER main.c lexer.c parser.c sematic.c intercode.c intercode2.c asmgen.c amd64.c aarch64.c utils.c err.c debugutil.c)
target_compile_options(GMS_COMPILER PRIVATE -std=c11 -g -gdwarf-4 -Wall -Wextra -DDEBUG  -fno-omit-frame-pointer -O0 -D_AMD64 )
# -fsanitize=address
if(CMAKE_HOST_UNIX)
elseif(CMAKE_HOST_WIN32)
  add_compile_options(-D_AMD64)
endif()
target_link_options(GMS_COMPILER PRIVATE )
#  -fsanitize=address

