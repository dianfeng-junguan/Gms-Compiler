#include "intercode.h"
#include "string.h"
#include "err.h"
#include "asmgen.h"
#include "assert.h"
#include "stdlib.h"
#include "stdbool.h"
#include "utils.h"
#include <stddef.h>
#include <stdio.h>
int tmpv_alloc_reg(list_t *tmpv_table, tmpvar_t tmpname, size_t reg_num) {
  size_t toalloc=0;
  for (size_t i=0; i < tmpv_table->len; ++i) {
    tmpvar_alloc_info_t* p=list_get(tmpv_table, i);
    if(p->tmpv_index==tmpname.index){
      return -1;
    } else if (p->reg == toalloc) {
      // this register is already allocated      
      toalloc++;
    }
  }
  //
  if (toalloc==reg_num) {
    // full
    do_log(REGULAR, ASMGEN_ALLOCREG, "register full while allocating tmpvar\n");
    return -1;
  }
  tmpvar_alloc_info_t newalloc = {
      .tmpv_index = tmpname.index, .reg = toalloc, .onstack = 0};
  append(tmpv_table, &newalloc);
  do_log(VERBOSE, ASMGEN_ALLOCREG, "allocated register %zu to tmp@%d\n",
         toalloc, tmpname.index);  
  return toalloc;
}
size_t tmpv_alloc_stack(list_t *tmpv_table, tmpvar_t tmpname, stackframe_t* sf) {
  for (size_t i=0; i < tmpv_table->len; ++i) {
    tmpvar_alloc_info_t* p=list_get(tmpv_table, i);
    if(p->tmpv_index==tmpname.index){
      return -1;
    }
  }
  char tmpvname[32];
  sprintf(tmpvname, "tmp@%d",tmpname.index);
  size_t offset=add_local(sf, tmpvname, tmpname.size);
  tmpvar_alloc_info_t newalloc = {
      .tmpv_index = tmpname.index, .offset=offset, .onstack = 1};
  append(tmpv_table, &newalloc);
  return offset;
}
void free_tmpvar(list_t* tmpv_table, tmpvar_t tmpname, stackframe_t *sf){
  for (size_t i=0; i < tmpv_table->len; ++i) {
    tmpvar_alloc_info_t* p=list_get(tmpv_table, i);
    if (p->tmpv_index == tmpname.index) {
      do_log(VERBOSE, ASMGEN_ALLOCREG, "freed tmpvar tmp@%zu\n",tmpname.index);
      list_remove(tmpv_table, i);
      if (p->onstack) {
        /* TODO: free stackspace */
        char tmpvarname[32];
	sprintf(tmpvarname, "tmp@%d",tmpname.index);
	remove_local(sf, tmpvarname);
      }
      return;
    }
  }
}

tmpvar_alloc_info_t* get_tmpv_alloc(list_t *tmpv_table, int tmpv_index){
  for (size_t i=0; i<tmpv_table->len; i++) {
    tmpvar_alloc_info_t *pair=list_get(tmpv_table, i);
    if(pair->tmpv_index== tmpv_index){
      return pair;
    }
  }
  return NULL;
}
bool is_reg_used(list_t *tmpv_table, int regno){
  for (size_t i=0; i<tmpv_table->len; i++) {
    tmpvar_alloc_info_t *pair=list_get(tmpv_table, i);
    if (pair->onstack == 0 && pair->reg == regno &&
        pair->tmpv_index != TMPVAR_INDEX_NULL) {      
      return true;
    }
  }
  return false;
}
void free_reg_str_pair(tmpvar_alloc_info_t *p) {}


static abi_t abis[] = {
    {.type = ABI_SYSTEMV,
     .arg_regs = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"},
     .arg_regs_num = 6,
     .caller_saved_regs = {"rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10",
                           "r11"},
     .caller_saved_regs_num = 9,
     .callee_saved_regs = {"rbx", "rbp", "rsp", "r12", "r13", "r14", "r15"},
     .callee_saved_regs_num = 7,
     .ret_reg = "rax"},
    {
        .type = ABI_MICROSOFT,
        .arg_regs = {"rcx", "rdx", "r8", "r9"},
        .arg_regs_num = 4,
        .caller_saved_regs = {"rax", "rcx", "rdx", "r8", "r9", "r10", "r11"},
        .caller_saved_regs_num = 7,
        .callee_saved_regs = {"rbx", "rbp", "rdi", "rsi", "r12", "r13", "r14",
                              "r15"},
        .callee_saved_regs_num = 8,
        .ret_reg = "rax",
        },
    {
        .type = ABI_AARCH64,
        .arg_regs =
            {
                "x0",
                "x1",
                "x2",
                "x3",
                "x4",
                "x5",
                "x6",
                "x7",
            },
        .arg_regs_num = 8,
        .caller_saved_regs = {"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
                              "x8", "x9", "x10", "x11", "x12", "x13", "x14",
                              "x15", "x16", "x17", "x18"},
        .caller_saved_regs_num = 19,
        .callee_saved_regs = {"x19", "x20", "x21", "x22", "x23", "x24", "x25",
                              "x26", "x27", "x28", "x29", "x30"},
        .callee_saved_regs_num = 12,
	.ret_reg = "x0"
    }
};
abi_t get_abi(abitype_t abi){
  for (size_t i=0; i<sizeof(abis)/sizeof(abi_t); i++) {
    if (abis[i].type==abi) {
      return abis[i];
    }
  }
  panic("met unknown abi\n");
}

stackframe_t create_stackframe(){
  stackframe_t stk = {
      .start = 0,
      .end = 0,
      .locals = create_list(10, sizeof(stackpos_local_t)),
  };
  return stk;  
}
size_t grow_stack(stackframe_t *stk,size_t size){
  size_t offset = stk->end;
  stk->end += size;
  return offset;  
}
void shrink_stack(stackframe_t *stk, size_t size) {
  assert(stk->end >= size);
  stk->end -= size;
}
size_t add_local(stackframe_t *stk, char *name, size_t var_size){
  stackpos_local_t localpos;
  size_t local_offset = grow_stack(stk, var_size);
  localpos.size = var_size;
  localpos.stack_offset = local_offset;
  cstring_t vnamestr = string_from(name);
  localpos.varname = vnamestr;
  append(&stk->locals, &localpos);
  return local_offset;
}
void remove_local(stackframe_t *stk, char *name) {
  for (size_t i=0; i < stk->locals.len; ++i) {
    stackpos_local_t *local = list_get(&stk->locals, i);
    if (strcmp(local->varname.data, name)==0) {
      list_remove(&stk->locals, i);
      return;
    }
  }
}
long long get_local_offset(stackframe_t* stk, char* name){
  for (size_t i=0; i < stk->locals.len; ++i) {
    stackpos_local_t *local = list_get(&stk->locals, i);
    if (strcmp(local->varname.data, name)==0) {
      return local->stack_offset;
    }
  }
  return -1;  
}
void clear_stack(stackframe_t* stk){
  stk->end = stk->start;
  for (size_t i=0; i<stk->locals.len; i++) {
    list_remove(&stk->locals, stk->locals.len-1-i);
  }
}
void free_stackframe(stackframe_t* sf){
  clear_stack(sf);
  free_list(&sf->locals);
}
