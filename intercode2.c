#include "intercode.h"
#include "utils.h"
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
  CODE(&ic2, CODE_DATA_SECTION ,0 ,0 ,0);
  // extract the global variable allocs
  for (size_t i=0; i < ic1->len; ++i) {
    intercode_t* code=list_get(ic1, i);
    if(code->type==CODE_ALLOC_GLOBAL){
      // put it to data section
      char* op2=code->operand2str;
      if(!op2){
	op2="0";
      }
      CODE(&ic2, CODE_GLOBAL_VAR_DATA, code->operand1str, op2, 0);
    }
    /* extract string constant to the data section */
#define EXTRACT_STRCONST(v)			\
    if(v&&*v&&v[0]=='\"'){			\
      char* strconst=make_tmp_label("const@");	\
      CODE(&ic2, CODE_DATA, strconst, v, 0);	\
      myfree(v);				\
      v=strconst;				\
    }
    EXTRACT_STRCONST(code->operand1str);
    EXTRACT_STRCONST(code->operand2str);
    EXTRACT_STRCONST(code->operand3str);
    
  }
  // now texts
  CODE(&ic2, CODE_TEXT_SECTION ,0 ,0 ,0);
  for (size_t i=0; i < ic1->len; ++i) {
    intercode_t* code=list_get(ic1, i);
    if(code->type==CODE_ALLOC_GLOBAL)continue;
    // copy other codes
    CODE(&ic2, code->type, code->operand1str, code->operand2str, code->operand3str);
  }
  return ic2;
}
