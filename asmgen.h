#include "utils.h"
#include <stddef.h>
#include <stdio.h>
typedef struct _tmpvar_t tmpvar_t;
#include <stdbool.h>
typedef struct {
  union{
    int reg;// the index of register in the register table
    size_t offset;//used for tmpvars on the stack
  };
  int tmpv_index;
  int onstack;
} tmpvar_alloc_info_t;

typedef enum {
  ABI_SYSTEMV,
  ABI_MICROSOFT,
  ABI_AARCH64,
} abitype_t;

typedef struct {
  abitype_t type;
  char* arg_regs[16];
  size_t arg_regs_num;
  char* caller_saved_regs[24];
  size_t caller_saved_regs_num;
  char* callee_saved_regs[24];
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
  int is_tmpvar;
  size_t stack_offset;
  size_t size;
  int tmpvar_index;
  cstring_t varname;
} stackpos_local_t;
/// the position vars in the struct is suggested to be interpreted as the offset from stack frame pointer. if so, the `start` will always be 0.
typedef struct _stackframe_t{
  size_t start;
  size_t end;
  // table (stackpos, local var)
  list_t locals;
} stackframe_t;
stackframe_t create_stackframe();
void free_stackframe(stackframe_t* sf);
/// grow the stackframe. returns the offset of the new alloced space
size_t grow_stack(stackframe_t* stk, size_t size);
void shrink_stack(stackframe_t *stk, size_t size);
void clear_stack(stackframe_t* stk);
/// alloc stack space for local variable and return the offset on the stack of the space alloced.
size_t add_local(stackframe_t *stk, char *name, size_t var_size);
void remove_local(stackframe_t *stk, char *name);
size_t stackframe_add_tmpvar(stackframe_t *stk, tmpvar_t tmpvar);
void stackframe_remove_tmpvar(stackframe_t *stk, int tmpvar_index);
long long stackframe_get_tmpvar_offset(stackframe_t *stk, int tmpvar_index);
tmpvar_alloc_info_t* get_tmpv_alloc(list_t *tmpv_table, int tmpv_index);
/// get the stack offset of the allocated varname. if the var cannot be found, it returns -1.
long long get_local_offset(stackframe_t* stk, char* name);

int tmpv_alloc_reg(list_t *tmpv_table, tmpvar_t tmpv, size_t reg_num);
size_t tmpv_alloc_stack(list_t *tmpv_table, tmpvar_t tmpname, stackframe_t* sf);
bool is_reg_used(list_t *tmpv_table,int regno);
void free_tmpvar(list_t *tmpv_table, tmpvar_t varname, stackframe_t *sf);
void free_reg_str_pair(tmpvar_alloc_info_t *p);

abi_t get_abi(abitype_t abi);
char* get_reg(list_t *regs,tmpvar_t varname);

char *amd64_gen(list_t *intercodes,platform_info_t arch);
char *aarch64_gen(list_t *intercodes, platform_info_t arch);

#define ASM(fmt, ...)				\
  do {						\
    char *line = malloc(128);			\
    assert(line);				\
    snprintf(line, 128, fmt, ##__VA_ARGS__);	\
    list_append(list_asm, &line);		\
  } while (0);
