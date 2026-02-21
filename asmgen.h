#include "utils.h"
#include <stddef.h>
#include <stdio.h>
typedef struct _tmpvar_t tmpvar_t;
#include <stdbool.h>
typedef struct {
  char reg[4];
  int var;
} reg_tmpvar_pair_t;

char *alloc_reg(list_t *regs, tmpvar_t varname);
bool is_reg_used(list_t *regs,char* regname);
void free_reg(list_t *regs, tmpvar_t varname);
void free_reg_str_pair(reg_tmpvar_pair_t *p);
reg_tmpvar_pair_t* create_regvar(char* reg);
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
} abi_t;
typedef enum{
  ARCH_AMD64,
  ARCH_AARCH64
}arch_t;
/**
   informations about target platform.
 **/
typedef struct {
  arch_t architecture;
  abitype_t abi;
}platform_info_t;

typedef struct {
  size_t stack_offset;
  size_t size;
  cstring_t varname;
} stackpos_local_t;
/// the position vars in the struct is suggested to be interpreted as the offset from stack frame pointer. if so, the `start` will always be 0.
typedef struct {
  size_t start;
  size_t end;
  // table (stackpos, local var)
  list_t locals;
} stackframe_t;
stackframe_t create_stackframe();
void grow_stack(size_t size);
void shrink_stack(size_t size);
void add_local(stackframe_t *stk, char *name, size_t var_size);
void remove_local(stackframe_t *stk, char *name);
long long get_local_offset(stackframe_t* stk, char* name);

abi_t get_abi(abitype_t abi);
char* get_reg(list_t *regs,tmpvar_t varname);
#ifdef _AMD64
char *amd64_gen(list_t *intercodes,platform_info_t arch);
#else
char *aarch64_gen(list_t *intercodes, platform_info_t arch);
#endif
