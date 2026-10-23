cmake_minimum_required(VERSION 3.10)
project(GMS_COMPILER LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
add_executable(gen tool/gen.cpp)
if(MSVC)
  target_compile_options(gen PRIVATE /Zi /W4)
else()
  target_compile_options(gen PRIVATE -g -Wall -Wextra)
endif()
add_custom_target(TOOLGEN COMMAND gen > debugutil.c ALL
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating lexer and parser"
  VERBATIM
  DEPENDS gen
)
add_executable(GMS_COMPILER main.cpp lexer.cpp parser.cpp sematic.cpp sematic_user.cpp utils.c err.c status.cpp)
if(MSVC)
  target_compile_options(GMS_COMPILER PRIVATE /Zi /W4 /DDEBUG /D_AMD64 /Od /Oy- /fsanitize=address)
  target_link_options(GMS_COMPILER PRIVATE /fsanitize=address)
else()
  target_compile_options(GMS_COMPILER PRIVATE -g -gdwarf-4 -Wall -Wextra -DDEBUG -fno-omit-frame-pointer -O0 -D_AMD64 )#-fsanitize=address
  # target_link_options(GMS_COMPILER PRIVATE -fsanitize=address)
endif()
#
if(CMAKE_HOST_UNIX)
elseif(CMAKE_HOST_WIN32)
  add_compile_options(-D_AMD64)
endif()
#  -fsanitize=address

