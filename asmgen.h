#include "utils.h"

typedef struct {
  char reg[4];
  char* var;
} reg_tmpvar_pair_t;

char* asm_gen(list_t* intercodes);
