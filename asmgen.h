#include "utils.h"

typedef struct {
  char reg[4];
  char* var;
} reg_tmpvar_pair_t;

char *alloc_reg(list_t *regs, char *varname);
void free_reg(list_t *regs, char *varname);
void free_reg_str_pair(reg_tmpvar_pair_t *p);
reg_tmpvar_pair_t* create_regvar(char* reg);
#ifdef _AMD64
char *amd64_gen(list_t *intercodes);
#else
char *aarch64_gen(list_t *intercodes);
#endif
typedef enum { ABI_SYSTEMV, ABI_MICROSOFT } abitype_t;

typedef struct {
  abitype_t type;
  char* arg_regs[12];
  size_t arg_regs_num;
  char* caller_saved_regs[12];
  size_t caller_saved_regs_num;
  char* callee_saved_regs[12];
  size_t callee_saved_regs_num;
  char ret_reg[10];
}abi_t;
abi_t get_abi(abitype_t abi);
