#include "asmgen.h"
#include "err.h"
#include "utils.h"
#include <assert.h>
#include <corecrt.h>
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
    list_append(list_asm, &line);                                               \
  } while (0);
static size_t stk_argsz = 0;
static char* format_operand(operand_t op,list_t* reg_table){
   if(!op.value)return NULL;
  switch (op.type) {
  case OPERAND_TMPVAR: {
    for (size_t i=0; i < reg_table->len; ++i) {
      reg_tmpvar_pair_t* pair=list_get(reg_table, i);
      if(pair->var&&strcmp(op.value,pair->var)==0){
	return pair->reg;
      }
    }
    //cry_errorf(SENDER_ASMGEN, ((filepos_t){0,0}), "tmpvar used before alloced");
    break;
  }
  default:break;
  }
  return op.value;
}
void amd64_translate(list_t* list_asm, intercode_t* intercode, size_t *stack_subbase, list_t *reg_table){
  char* op1=format_operand(intercode->op1, reg_table);
  char* op2=format_operand(intercode->op2, reg_table);
  char* op3=format_operand(intercode->op3, reg_table);
  switch (intercode->type) {
  case CODE_DATA_SECTION:
    ASM("section .data\n");
    break;
  case CODE_TEXT_SECTION:
    ASM("section .text\n");
    break;
  case CODE_GLOBAL_VAR_DATA:
    ASM("%s: dq %s\n%%define %s qword [%s]\n",op1, op2,
	op1, op1);
    break;
  case CODE_DATA:{
    char* data=op2;
    char* width="dq";
    char* ending="";
    if(data[0]=='\"'){
      width="db";
      ending=",0";
    }
    ASM("%s: %s %s%s\n",op1, width, op2, ending);
    break;
  }
  case CODE_DEF_FUNC: {
    ASM("global %s\n",op1);
    ASM("%s:\n",op1);
    ASM("push rbp\nmov rbp,rsp\n");    
    break;
  }
  case CODE_RETURN: {
    if(op1){
      ASM("mov rax,%s\n",op1);
    }
    ASM("mov rsp,rbp\npop rbp\nret\n");
    break;
  }
  case CODE_ALLOC_GLOBAL:{
    char* initv=op2;
    if(!initv)initv="0";
    ASM("%s: dq %s\n",op1,initv);
    ASM("%%define %s qword [%s]\n",op1, op1);
    break;
  }
  case CODE_ALLOC_LOCAL: {
    ASM("sub rsp,%s\n",op2);
    size_t var_size=atoi(op2);
    *stack_subbase=*stack_subbase+var_size;
    ASM("%%define %s qword [rbp-%zu]\n",op1,*stack_subbase);    
    break;
  }
  case CODE_ALLOC_TMP: {
    // todo: alloc registers accordingly
    char* reg=alloc_reg(reg_table, op1);
    //ASM("%%define %s %s\n",op1,reg);
    break;
  }    
#define TWOOP(codenoprefix,ins)						\
    case CODE_##codenoprefix: {						\
      ASM("mov %s,%s\n%s %s,%s\n",op3,op1, \
	  #ins,op3,op2);		\
      break;								\
    }
    
    TWOOP(ADD, add);
    TWOOP(SUB, sub);
    TWOOP(BITAND, and);
    TWOOP(BITOR, or);
    TWOOP(BITNOT, not);    
    
  case CODE_MUL: {
    ASM("push rdx\npush rax\nmov rdx,0\nmov rax,%s\n",op1);
    ASM("mul %s\n",op2);
    ASM("mov %s,rax\n",op3);
    ASM("pop rax\npop rdx\n");
    break;
  }
  case CODE_DIV: {
    ASM("push rdx\npush rax\nmov rdx,0\nmov rax,%s\n",op1);
    ASM("div %s\n",op2);
    ASM("mov %s,rax\n",op3);
    ASM("pop rax\npop rdx\n");
    break;
    break;
  }
    
  case CODE_MOD: {
    ASM("push rdx\npush rax\nmov rdx,0\nmov rax,%s\n",op1);
    ASM("div %s\n",op2);
    ASM("mov %s,rdx\n",op3);
    ASM("pop rax\npop rdx\n");
    break;
    break;
  }
  case CODE_CMP: {
    ASM("cmp %s,%s\n",op1, op2);
    break;
  }
  case CODE_DECLARE: {
    // do nothing
    break;
  }
  case CODE_EXTERN_DECLARE: {
    ASM("extern %s\n",op1);
    break;
  }
#define JMP(codenoprefix)					\
    case CODE_##codenoprefix: {					\
      ASM("%s near .%s\n",#codenoprefix,op1);	\
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
    ASM(".%s:\n",op1);
    break;
  }
  case CODE_M2M:
    ASM("push rax\nmov rax,%s\nmov %s,rax\npop rax\n",op2, op1);
    break;
  case CODE_LOAD: 
  case CODE_STORE: 
  case CODE_MOV: {
    ASM("mov %s,%s\n",op1, op2);
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
    free_reg(reg_table, op1);
    break;
  }
  case CODE_PUSHARG: {
    // push arg
    ASM("push %s\n",op1);
    stk_argsz+=8;
    break;
  }
  case CODE_FUNCCALL: {
    /* to pass args. we store the value of tmpvars into stack and then mov it to the registers before
     calling the func*/
    /* TODO: auto select abi */
    abi_t abi=get_abi(ABI_MICROSOFT);
    char* ret_reg=op2;
    // save used caller-saved regs but the reg used to store return value
    size_t pushed_regs_num=0;
    for (size_t i=0; i<abi.caller_saved_regs_num; i++) {
      if(is_reg_used(reg_table, abi.caller_saved_regs[i])&&strcmp(ret_reg,abi.caller_saved_regs[i])!=0){
	ASM("push %s\n",abi.caller_saved_regs[i]);
	pushed_regs_num++;
      }
    }
    for (size_t i=0; i < stk_argsz/8; ++i) {
      // we copy the part that overflows registers reversely
      // since the earlier pushargs are from left to right
      ASM("mov %s,[rsp+%zu+%zu]\n",(i<abi.arg_regs_num?abi.arg_regs[i]:"rax"),8*pushed_regs_num,i*8);
      if(i>=abi.arg_regs_num){
	// push the arg when num of args is more than arg registers
	ASM("push rax\n");
      }
    }
    ASM("call %s\n",op1);
    if(stk_argsz>8*abi.arg_regs_num){
      ASM("add rsp,%zu\n",stk_argsz-8*abi.arg_regs_num);
    }
    if(op2){
      // store the return value
      ASM("mov %s,rax\n",op2);
    }
    // restore the stacks used to pass args
    for (size_t i=abi.caller_saved_regs_num; i>0; i--) {
      if(is_reg_used(reg_table, abi.caller_saved_regs[i-1])&&strcmp(ret_reg,abi.caller_saved_regs[i-1])!=0){
	ASM("pop %s\n",abi.caller_saved_regs[i-1]);
      }
    }
    ASM("add rsp,%zu\n",stk_argsz);
    stk_argsz=0;
    break;
  }
  case CODE_STORE_RETV: {
    break;
  }
  default:
    cry_errorf(SENDER_ASMGEN, (filepos_t){0},
	       "fatal: met unrecognized intercode %s.",codetype_tostr(intercode->type));
    exit(-1);
    break;
  }
}
#define ASMU(codes,fmt, ...)			\
  do {						\
    char *line = malloc(64);			\
    assert(line);				\
    snprintf(line, 64, fmt, ##__VA_ARGS__);	\
    list_append(codes, &line);			\
  } while (0);
char* amd64_gen(list_t* intercodes){
  list_t asm_codes=create_list(20, sizeof(char*));
  size_t stk=0;
  list_t reg_table=create_list(6, sizeof(reg_tmpvar_pair_t));
  abi_t abi=get_abi(ABI_MICROSOFT);
  for (size_t i=0; i<abi.caller_saved_regs_num; i++) {
    list_append(&reg_table, create_regvar(abi.caller_saved_regs[i]));
  }
  // something
  ASMU(&asm_codes, "bits 64\n");
  ASMU(&asm_codes, "default rel\n");
  
  for (size_t i=0; i < intercodes->len; ++i) {
   amd64_translate(&asm_codes, list_get(intercodes, i),&stk,&reg_table);    
  }
  // now concat the codes
  size_t nowcapa=128;
  size_t nowsize=0;
  char* result=malloc(nowcapa);
  for (size_t i=0; i < asm_codes.len; ++i) {
    char* line=*(char**)list_get(&asm_codes, i);
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
  for (size_t i=0; i < asm_codes.len; ++i) {
    char* line=*(char**)list_get(&asm_codes, i);
    free(line);
  }
  free_list(&asm_codes);
  FREE_LIST_DTOR(&reg_table, free_reg_str_pair);
  return result;
}
