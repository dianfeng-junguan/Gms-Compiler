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
    snprintf(line, 64, fmt, ##__VA_ARGS__);				\
    list_append(list_asm, line);                                               \
  } while (0);
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

static size_t stk_argsz=0;
void asm_translate(list_t* list_asm, intercode_t* intercode, size_t *stack_subbase, list_t *reg_table){
  switch (intercode->type) {
  case CODE_DEF_FUNC: {
    ASM("global %s\n",intercode->label);
    ASM("%s:\n",intercode->label);
    ASM("push rbp\nmov rbp,rsp\n");    
    break;
  }
  case CODE_RETURN: {
    if(intercode->operand1){
      ASM("mov rax,%s\n",intercode->operand1str);
    }
    ASM("mov rsp,rbp\npop rbp\nret\n");
    break;
  }
  case CODE_ALLOC_GLOBAL:{
    char* initv=intercode->operand2str;
    if(!initv)initv="0";
    ASM("%s: dq %s\n",intercode->label,initv);
    ASM("%%define %s qword [%s]\n",intercode->label, intercode->label);
    break;
  }
  case CODE_ALLOC_LOCAL: {
    ASM("sub rsp,%s\n",intercode->operand2str);
    size_t var_size=atoi(intercode->operand2str);
    *stack_subbase=*stack_subbase+var_size;
    ASM("%%define %s qword [rbp-%zu]\n",intercode->operand1str,*stack_subbase);    
    break;
  }
  case CODE_ALLOC_TMP: {
    // todo: alloc registers accordingly
    char* reg=alloc_reg(reg_table, intercode->operand1str);
    ASM("%%define %s %s\n",intercode->operand1str,reg);
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
  case CODE_DECLARE: {
    // do nothing
    break;
  }
  case CODE_EXTERN_DECLARE: {
    ASM("extern %s\n",intercode->operand1str);
    break;
  }
#define JMP(codenoprefix)					\
    case CODE_##codenoprefix: {					\
      ASM("%s near %s\n",#codenoprefix,intercode->operand1str);	\
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
    // push arg
    ASM("push %s\n",intercode->operand1str);
    stk_argsz+=8;
    break;
  }
  case CODE_FUNCCALL: {
    /* to pass args. we store the value of tmpvars into stack and then mov it to the registers before
     calling the func*/
    ASM("push rdi\npush rsi\npush rdx\npush rcx\npush r8\npush r9\n");
    char* regs[]={"rdi","rsi","rdx","rcx","r8","r9"};
    for (size_t i=0; i < stk_argsz/8; ++i) {
      // we copy the part that overflows registers reversely to meet system V
      // since the earlier pushargs are from left to right
      ASM("mov %s,[rsp+48+%zu]\n",(i<6?regs[i]:"rax"),i*8);
      if(i>=6){
	ASM("push rax\n");
      }
    }
    ASM("call %s\n",intercode->operand1str);
    if(stk_argsz>48){
      ASM("add rsp,%zu\n",stk_argsz-48);
    }
    // restore the stacks used to pass args
    ASM("pop r9\npop r8\npop rcx\npop rdx\npop rsi\npop rdi\n");
    ASM("add rsp,%zu\n",stk_argsz);
    stk_argsz=0;
    break;
  }
  case CODE_STORE_RETV: {
    ASM("mov %s,rax\n",intercode->operand1str);
    break;
  }
  default:
    cry_errorf(SENDER_ASMGEN, (filepos_t){0},
	       "fatal: met unrecognized intercode %s.",codetype_tostr(intercode->type));
    exit(-1);
    break;
  }
}
reg_tmpvar_pair_t* create_regvar(char* reg){
  reg_tmpvar_pair_t* stru=malloc(sizeof(reg_tmpvar_pair_t));
  assert(stru);
  strcpy(stru->reg,reg);
  stru->var=NULL;
  return stru;
}
#define ASMU(codes,fmt, ...)			\
  do {						\
    char *line = malloc(64);			\
    assert(line);				\
    snprintf(line, 64, fmt, ##__VA_ARGS__);	\
    list_append(codes, line);			\
  } while (0);
char* asm_gen(list_t* intercodes){
  list_t asm_codes=create_list(20, sizeof(char*));
  size_t stk=0;
  list_t reg_table=create_list(6, sizeof(reg_tmpvar_pair_t));
  list_append(&reg_table, create_regvar("rax"));
  list_append(&reg_table, create_regvar("rbx"));
  list_append(&reg_table, create_regvar("rcx"));
  list_append(&reg_table, create_regvar("rdx"));
  list_append(&reg_table, create_regvar("rdi"));
  list_append(&reg_table, create_regvar("rsi"));
  // something
  ASMU(&asm_codes, "bits 64\n");
  ASMU(&asm_codes, "default rel\n");  
  // first move the global variables to the data section
  ASMU(&asm_codes,"[section .data]\n");
  for (size_t i=0; i < intercodes->len; ++i) {
    intercode_t* code=list_get(intercodes, i);
    if(code->type==CODE_ALLOC_GLOBAL){
      asm_translate(&asm_codes, code, &stk, &reg_table);
      //free_intercode(code);
      list_remove_shallow(intercodes, i);
    }
  }
  ASMU(&asm_codes, "[section .text]\n");
  for (size_t i=0; i < intercodes->len; ++i) {
    asm_translate(&asm_codes, list_get(intercodes, i),&stk,&reg_table);    
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
  // free the previous line str
  free_list(&asm_codes);
  free_list(&reg_table);
  return result;
}
