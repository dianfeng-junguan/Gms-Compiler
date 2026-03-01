#include "intercode.h"
#include "utils.h"
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "intercode2.h"

size_t used_tmp_labels = 0;
char* make_tmp_label(char* prefix){
  size_t prefl=strlen(prefix);
  size_t dig=0;
  size_t tmpc=used_tmp_labels;
  while (tmpc>0) {
    tmpc/=10;
    dig++;
  }
  char* lbl=myalloc(prefl+dig+1);
  strcpy(lbl, prefix);
  size_t tmpc2=used_tmp_labels;
  for (size_t i=0; i < dig; ++i) {
    size_t d=tmpc2%10;
    tmpc2/=10;
    lbl[prefl+dig-1-i]='0'+d;
  }
  used_tmp_labels++;
  return lbl;
}
/**
   process generated intercodes.
   it arrages global var allocs and string constants to data section.
 **/
list_t process_intercode(list_t* ic1){
  list_t ic2=create_list(100, sizeof(intercode_t));
  // create the data section first
  CODE(&ic2, CODE_DATA_SECTION ,EMPTY ,EMPTY ,EMPTY);
  // extract the global variable allocs
  for (size_t i=0; i < ic1->len; ++i) {
    intercode_t* code=list_get(ic1, i);
    if(code->type==CODE_ALLOC_GLOBAL){
      // put it to data section
      operand_t op2=code->op2;
      if(op2.type==OPERAND_EMPTY){
	op2=IMM("0");
      }
      CODE(&ic2, CODE_GLOBAL_VAR_DATA, code->op1, op2, EMPTY);
    }
    /* extract string constant to the data section */
#define EXTRACT_STRCONST(v)			\
    if(v&&v[0]=='\"'){				\
      char* strconst=make_tmp_label("const__");	\
      CODE(&ic2, CODE_DATA, ADDR(strconst), IMM(v), EMPTY);	\
      myfree(v);				\
      v=strconst;				\
    }
    EXTRACT_STRCONST(code->op1.value);
    EXTRACT_STRCONST(code->op2.value);
    EXTRACT_STRCONST(code->op3.value);
    
  }
  // this macro is to see whether the operand is a tmpvar or immediate number.
  // tmpvar and immediate var can be transferred to tmpvar and mem,
  // while mem can only be transferred to tmpvar
  // FIXME: replace isdigit with better method to judge if it is a number that can be treated as immediate
#define IS_TMPVAR_OR_IMM(op) (op.type==OPERAND_TMPVAR||op.type==OPERAND_IMMEDIATE)
  // now texts
  push_code(&ic2, CODE_TEXT_SECTION ,EMPTY ,EMPTY ,EMPTY);
  for (size_t i=0; i < ic1->len; ++i) {
    intercode_t *code = list_get(ic1, i);
    push_code(&ic2, code->type, code->op1, code->op2, code->op3);    
    /*if(code->type==CODE_ALLOC_GLOBAL)continue;
    else if(code->type==CODE_MOV){
      // if mem->tmpvar
      if(IS_TMPVAR_OR_IMM(code->op1)&&!IS_TMPVAR_OR_IMM(code->op2)){
        push_code(&ic2, CODE_LOAD, code->op1, code->op2, code->op3);
      }else if(!IS_TMPVAR_OR_IMM(code->op1)&&IS_TMPVAR_OR_IMM(code->op2)){
        // tmpvar->mem
      push_code(&ic2, CODE_STORE, code->op1, code->op2, code->op3);
      }else if(IS_TMPVAR_OR_IMM(code->op1)&&IS_TMPVAR_OR_IMM(code->op2)){
        // tmpvar->tmpvar
        push_code(&ic2, CODE_MOV, code->op1, code->op2, code->op3);
      }else{
        // mem->mem
         push_code(&ic2, CODE_M2M, code->op1, code->op2, code->op3);
      }
    }else{
      // copy other codes
       
      }*/
    }
  
  // calculate the livespan of tmpvar and insert free code accordingly
  typedef struct{
    int tmpvar_index;
    size_t last_used_index;
  }tmpvar_livespan_t;
  list_t collected_tmpvars = create_list(50, sizeof(tmpvar_livespan_t));
  bool used_this_tmpvar(int tmpvar_index, operand_t op);
  // collect tmpvars
  // for each tmpvar, find the last usage of it and insert free code after it.  
  for(size_t i=0;i<ic2.len;i++) {    
    intercode_t *code = list_get(&ic2, i);
    if (code->type == CODE_ALLOC_TMP) {
      tmpvar_livespan_t span={.tmpvar_index=code->op1.tmpvalue.index, .last_used_index=i};
      append(&collected_tmpvars, &span);
    } else {
      // check whether this instructon used tmpvars      
      for (size_t j = 0; j < collected_tmpvars.len; ++j) {
	tmpvar_livespan_t* span=list_get(&collected_tmpvars, j);
	if (used_this_tmpvar(span->tmpvar_index, code->op1) ||
	    used_this_tmpvar(span->tmpvar_index, code->op2) ||
	    used_this_tmpvar(span->tmpvar_index, code->op3)) {
	  // used. update the last_used_index.          
	  span->last_used_index=i;
	}
      }
    }
  }
  // sort the list by descending order
  for (size_t i=0; i<collected_tmpvars.len; i++) {
    for (size_t j=1; j < collected_tmpvars.len-i; ++j) {
      tmpvar_livespan_t* l1=list_get(&collected_tmpvars, j-1);
      tmpvar_livespan_t *l2 = list_get(&collected_tmpvars, j);
      if (l1->last_used_index < l2->last_used_index) {
	list_swap(&collected_tmpvars, j-1, j);
      }
    }
  }
  for (size_t i = 0; i < collected_tmpvars.len; i++) {
    tmpvar_livespan_t *span = list_get(&collected_tmpvars, i);
    //printf("livespan{index=%d, last_used=%zu}\n",span->tmpvar_index,span->last_used_index);
    intercode_t code =
        create_code(CODE_FREE, TMP({.index=span->tmpvar_index}), EMPTY, EMPTY);
    list_insert(&ic2, span->last_used_index + 1, &code);
  }
  free_list(&collected_tmpvars);  
  free_list(ic1);  
  return ic2;
}
bool used_this_tmpvar(int tmpvar_index, operand_t op){
  return (op.type==OPERAND_OFFSET||op.type == OPERAND_TMPVAR) && op.tmpvalue.index == tmpvar_index;
}
