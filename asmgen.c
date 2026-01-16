#include "asmgen.h"
#include "err.h"
#include "utils.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "intercode.h"
#define ASM(fmt, ...)                                                          \
  do {                                                                         \
    char *line = malloc(64);                                                   \
    assert(line);                                                              \
    sprintf(line, fmt, ##__VA_ARGS__);                                         \
    list_append(list_asm, line);                                               \
  } while (0);

void asm_translate(list_t* list_asm, intercode_t* intercode, size_t *stack_subbase){
  switch (intercode->type) {
  case CODE_DEF_FUNC: {
    ASM("global %s\n",intercode->label);
    ASM("%s:\n",intercode->label);
    ASM("push rbp\nmov rbp,rsp\n");    
    break;
  }
  case CODE_RETURN: {
    ASM("mov rsp,rbp\npop rbp\nret\n");
    break;
  }
  case CODE_ALLOC_GLOBAL:{
    ASM("%s: dq %s\n",intercode->label,intercode->operand2str);
    break;
  }
  case CODE_ALLOC_LOCAL: {
    ASM("sub rsp,%s\n",intercode->operand2str);
    size_t var_size=atoi(intercode->operand2str);
    *stack_subbase=*stack_subbase+var_size;
    ASM("%%define %s [rbp-%zu]\n",intercode->operand1str,*stack_subbase);    
    break;
  }
  case CODE_ALLOC_TMP: {
    // todo: alloc registers accordingly
    break;
  }    
#define TWOOP(codenoprefix,ins)						\
    case CODE_##codenoprefix: {						\
      ASM("mov %s,%s\n%s %s,%s\n",intercode->operand3str,intercode->operand1str, \
	  #ins,intercode->operand3str,intercode->operand2str);		\
      break;								\
    }
    
    TWOOP(ADD, add);
    TWOOP(SUB, sub);
    TWOOP(BITAND, and);
    TWOOP(BITOR, or);
    TWOOP(BITNOT, not);    
    
  case CODE_MUL: {
    ASM("mov %s,%s\n",intercode->operand3str, intercode->operand1str);
    ASM("push rdx\npush rax\nmov rdx,0\nmov rax,%s\n",intercode->operand2str);
    ASM("mul %s\n",intercode->operand2str);
    ASM("mov %s,rax\n",intercode->operand3str);
    ASM("pop rax\npop rdx\n");
    break;
  }
  case CODE_DIV: {
    ASM("mov %s,%s\n",intercode->operand3str, intercode->operand1str);
    ASM("push rdx\npush rax\nmov rdx,0\nmov rax,%s\n",intercode->operand2str);
    ASM("div %s\n",intercode->operand2str);
    ASM("mov %s,rax\n",intercode->operand3str);
    ASM("pop rax\npop rdx\n");
    break;
    break;
  }
    
  case CODE_MOD: {
    ASM("mov %s,%s\n",intercode->operand3str, intercode->operand1str);
    ASM("push rdx\npush rax\nmov rdx,0\nmov rax,%s\n",intercode->operand2str);
    ASM("div %s\n",intercode->operand2str);
    ASM("mov %s,rdx\n",intercode->operand3str);
    ASM("pop rax\npop rdx\n");
    break;
    break;
  }
  case CODE_CMP: {
    ASM("cmp %s,%s\n",intercode->operand1str, intercode->operand2str);
    break;
  }
#define JMP(codenoprefix)					\
    case CODE_##codenoprefix: {					\
      ASM("%s %s\n",#codenoprefix,intercode->operand1str);	\
      break;							\
  }
    JMP(JE);
    JMP(JNE);
    JMP(JA);
    JMP(JB);
    JMP(JAE);
    JMP(JBE);
    JMP(JMP);
  case CODE_LABEL: {
    ASM("%s:\n",intercode->label);
    break;
  }
  case CODE_MOV: {
    ASM("mov %s,%s\n",intercode->operand1str, intercode->operand2str);
    break;
  }
  case CODE_SCOPE_END: {
    // actually we do not really need to free the stack alloced by the deeper scope.
    // when func returns, they will all be freed.
    break;
  }
    
  case CODE_DEF_FUNC_END: {
    *stack_subbase=0;
    break;
  }
  case CODE_FREE: {
    // useful for tempvars
    // todo: free tmpvars
    break;
  }
  case CODE_PUSHARG: {
    // todo: push arg
    break;
  }
  case CODE_FUNCCALL: {
    ASM("call %s\n",intercode->operand1str);
    break;
  }
  default:
    cry_errorf(SENDER_ASMGEN, (filepos_t){0},
	       "fatal: met unrecognized intercode %s.",codetype_tostr(intercode->type));
    exit(-1);
    break;
  }
}
char* asm_gen(list_t* intercodes){
  list_t asm_codes=create_list(20, sizeof(char*));
  size_t stk=0;
  for (size_t i=0; i < intercodes->len; ++i) {
    asm_translate(&asm_codes, list_get(intercodes, i),&stk);
  }
  // now concat the codes
  size_t nowcapa=128;
  size_t nowsize=0;
  char* result=malloc(nowcapa);
  for (size_t i=0; i < asm_codes.len; ++i) {
    char* line=list_get(&asm_codes, i);
    size_t linel=strlen(line);
    // extend
    while(nowsize+linel>=nowcapa){
      nowcapa*=2;
      result=realloc(result, nowcapa);      
    }
    //copy
    strcpy(result+nowsize, line);
    nowsize+=linel;
  }
  return result;
}
