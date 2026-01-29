#include "string.h"
#include "err.h"
#include "asmgen.h"
#include "assert.h"
#include "stdlib.h"
#include "stdbool.h"
char *alloc_reg(list_t* regs, char* varname) {
  for (size_t i=0; i < regs->len; ++i) {
    reg_tmpvar_pair_t* p=list_get(regs, i);
    if(!p->var||strcmp(p->var,varname)==0){
      p->var=varname;
      return p->reg;
    }
  }
  cry_error(SENDER_ASMGEN, "registers full while allocating temporary varaibles", (filepos_t){0});
  return NULL;
}
void free_reg(list_t* regs, char* varname){
  for (size_t i=0; i < regs->len; ++i) {
    reg_tmpvar_pair_t* p=list_get(regs, i);
    if(p->var&&strcmp(p->var,varname)==0){
      p->var=NULL;      
    }
  }
}

char* get_reg(list_t *regs,char* varname){
  for (size_t i=0; i<regs->len; i++) {
    reg_tmpvar_pair_t *pair=list_get(regs, i);
    if(pair->var&&strcmp(pair->var, varname)==0){
      return pair->reg;
    }
  }
  return NULL;
}
bool is_reg_used(list_t *regs,char* regname){
  for (size_t i=0; i<regs->len; i++) {
    reg_tmpvar_pair_t *pair=list_get(regs, i);
    if(strcmp(pair->reg, regname)==0&&pair->var){
      return true;
    }
  }
  return false;
}
void free_reg_str_pair(reg_tmpvar_pair_t *p) {}

reg_tmpvar_pair_t* create_regvar(char* reg){
  reg_tmpvar_pair_t* stru=malloc(sizeof(reg_tmpvar_pair_t));
  assert(stru);
  strcpy(stru->reg,reg);
  stru->var=NULL;
  return stru;
}

static abi_t abis[]={
  {
    .type=ABI_SYSTEMV,
    .arg_regs={"rdi", "rsi", "rdx", "rcx", "r8", "r9"},
    .arg_regs_num=6,
    .caller_saved_regs={
      "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"
    },
    .caller_saved_regs_num=9,
    .callee_saved_regs={
      "rbx","rbp","rsp","r12", "r13", "r14", "r15"
    },
    .callee_saved_regs_num=7
  },
  {
    .type=ABI_MICROSOFT,
    .arg_regs={"rcx", "rdx", "r8", "r9"},
    .arg_regs_num=4,
    .caller_saved_regs={
      "rax","rcx","rdx","r8","r9","r10","r11"
    },
    .caller_saved_regs_num=7,
    .callee_saved_regs={
      "rbx", "rbp", "rdi", "rsi", "r12", "r13", "r14", "r15"
    },
    .callee_saved_regs_num=8
  },
};
abi_t get_abi(abitype_t abi){
  for (size_t i=0; i<sizeof(abis)/sizeof(abi_t); i++) {
    if (abis[i].type==abi) {
      return abis[i];
    }
  }
}
