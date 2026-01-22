#include "intercode.h"
#include "utils.h"
#include <ctype.h>
#include <stddef.h>
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
  // do the data section first
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
    if(v&&*v&&v[0]=='\"'){			\
      char* strconst=make_tmp_label("const@");	\
      CODE(&ic2, CODE_DATA, ADDR(strconst), ADDR(v), EMPTY);	\
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
#define IS_TMPVAR_OR_IMM(op) (op.value&&(op.value[0]=='@'||isdigit(op.value[0])))
  // now texts
  CODE(&ic2, CODE_TEXT_SECTION ,EMPTY ,EMPTY ,EMPTY);
  for (size_t i=0; i < ic1->len; ++i) {
    intercode_t* code=list_get(ic1, i);
    if(code->type==CODE_ALLOC_GLOBAL)continue;
    else if(code->type==CODE_MOV){
      // if mem->tmpvar
      if(IS_TMPVAR_OR_IMM(code->op1)&&!IS_TMPVAR_OR_IMM(code->op2)){
	CODE(&ic2, CODE_LOAD, code->op1, code->op2, code->op3);
      }else if(!IS_TMPVAR_OR_IMM(code->op1)&&IS_TMPVAR_OR_IMM(code->op2)){
	// tmpvar->mem
	CODE(&ic2, CODE_STORE, code->op1, code->op2, code->op3);
      }else if(IS_TMPVAR_OR_IMM(code->op1)&&IS_TMPVAR_OR_IMM(code->op2)){
	// tmpvar->tmpvar
	CODE(&ic2, CODE_MOV, code->op1, code->op2, code->op3);
      }else{
	// mem->mem
	CODE(&ic2, CODE_M2M, code->op1, code->op2, code->op3);
      }
    }else{
      // copy other codes
      CODE(&ic2, code->type, code->op1, code->op2, code->op3); 
    }
  }
  return ic2;
}
