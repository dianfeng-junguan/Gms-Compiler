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
    list_append(list_asm, &line);                                               \
  } while (0);
static size_t stk_argsz = 0;
static char* format_operand(operand_t op,list_t* reg_table){
  switch (op.type) {
  case OPERAND_TMPVAR: {
    for (size_t i=0; i < reg_table->len; ++i) {
      reg_tmpvar_pair_t* pair=list_get(reg_table, i);
      if(pair->var!=TMPVAR_INDEX_NULL&&op.tmpvalue.index==pair->var){
	return pair->reg;
      }
    }
    //cry_errorf(SENDER_ASMGEN, ((filepos_t){0,0}), "tmpvar used before alloced");
    break;
  }
  case OPERAND_OFFSET: {
    for (size_t i=0; i < reg_table->len; ++i) {
      reg_tmpvar_pair_t* pair=list_get(reg_table, i);
      if (pair->var > 0 && op.tmpvalue.index == pair->var) {
        cstring_t cstr = create_string();
	string_sprintf(&cstr, "[%s+%zu]",pair->reg,op.offset);
	return cstr.data;
      }
    }
    break;
  }
  default:break;
  }
  return op.value;
}
typedef struct {
  char* var;
  i64 offset;// rbp-based offset
} var_stackoffset_t;
/**
 * @brief      alloc a stack mem area to the var.
 *
 * @details    .
 *
 * @param      
 *
 * @return     void
 */
void alloc_stk(list_t *var_stkoffs, char* var, i64 offset){
  for (size_t i=0; i < var_stkoffs->len; ++i) {
    var_stackoffset_t *p=list_get(var_stkoffs, i);
    if(strcmp(p->var,var)==0){
      // duplicate
      cry_errorf(SENDER_ASMGEN, ((filepos_t){0,0}), "duplicate allocation of stack for var %s\n",var);
      return;
    }
  }
  append(var_stkoffs, &((var_stackoffset_t){.var=var,.offset=offset}));
}
void free_stk(list_t *var_stkoffs, char *var){
  for (size_t i=0; i < var_stkoffs->len; ++i) {
    var_stackoffset_t *p=list_get(var_stkoffs, i);
    if(strcmp(p->var,var)==0){
      list_remove(var_stkoffs, i);
      return;
    }
  }
}
i64 get_stkoff(list_t *var_stkoffs, char* var){
  for (size_t i=0; i < var_stkoffs->len; ++i) {
    var_stackoffset_t *p=list_get(var_stkoffs, i);
    if(strcmp(p->var,var)==0){
      return p->offset;
    }
  }
  cry_errorf(SENDER_ASMGEN, ((filepos_t){0}), "searching for a not-yet-alloced-stack var %s\n",var);
  return -1;
}
void amd64_translate(list_t* list_asm, intercode_t* intercode, size_t *stack_subbase, list_t *reg_table, platform_info_t arch, list_t *var_stkoffs){
  char* op1=format_operand(intercode->op1, reg_table);
  char* op2=format_operand(intercode->op2, reg_table);
  char *op3 = format_operand(intercode->op3, reg_table);
  abi_t abi=get_abi(arch.abi);
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
    for (size_t  i=0; i<abi.callee_saved_regs_num; i++) {
      ASM("push %s\n",abi.callee_saved_regs[i]);
    }
    break;
  }
  case CODE_RETURN: {
    if(op1){
      ASM("mov rax,%s\n",op1);
    }
    for (size_t i=0; i<abi.callee_saved_regs_num; i++) {
      ASM("pop %s\n",abi.callee_saved_regs[abi.callee_saved_regs_num-1-i]);
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
    //alloc_stk(var_stkoffs, op1, -*stack_subbase);
    ASM("%%define %s qword [rbp-%zu-%zu]\n",op1,8*abi.callee_saved_regs_num,*stack_subbase) ;    
    break;
  }
  case CODE_ALLOC_TMP: {
    // todo: alloc registers accordingly
    char *endptr;
    long tmpsz = strtol(op2, &endptr, 10);
    assert(endptr!=op2);
    if (tmpsz<=8) {
      char* reg=alloc_reg(reg_table, intercode->op1.tmpvalue);
    }else{
      // need to alloc stack space
      size_t aligned_size = (tmpsz + 7) & ~7;
      ASM("sub rsp,%zu\n", aligned_size);
      /* TODO: alloc stack space for tmpvar */
    }
    
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
  case CODE_REFER:{
    ASM("lea %s,%s\n",op1,op2);
    break;
  }
  case CODE_DEFER:{
    ASM("mov %s,%s\nmov %s,[%s]\n",op1,op2,op1,op1);
    break;
  }
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
    free_reg(reg_table, intercode->op1.tmpvalue);
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
    char *ret_reg = op2;
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
char* amd64_gen(list_t* intercodes,platform_info_t arch){
  list_t asm_codes=create_list(20, sizeof(char*));
  size_t stk=0;
  list_t reg_table=create_list(6, sizeof(reg_tmpvar_pair_t));
  list_t var_stkoffs=create_list(6, sizeof(var_stackoffset_t));
  abi_t abi=get_abi(arch.abi);
  for (size_t i=0; i<abi.caller_saved_regs_num; i++) {
    list_append(&reg_table, create_regvar(abi.caller_saved_regs[i]));
  }
  // something
  ASMU(&asm_codes, "bits 64\n");
  ASMU(&asm_codes, "default rel\n");
  
  for (size_t i=0; i < intercodes->len; ++i) {
    amd64_translate(&asm_codes, list_get(intercodes, i),&stk,&reg_table,arch,&var_stkoffs);    
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
