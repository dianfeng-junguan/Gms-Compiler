#include "utils.h"
#include <stdio.h>
#include "err.h"
void cry_error(const char* sender, char* msg, filepos_t pos){
  printf("Error from %s:%s at line %zu, column %zu\n",sender, msg, pos.line, pos.column);
}
