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
    char *line = malloc(128);                                                   \
    assert(line);                                                              \
    snprintf(line, 128, fmt, ##__VA_ARGS__);				\
    list_append(list_asm, &line);                                               \
  } while (0);

static size_t stk_argsz = 0;
static char* format_operand(operand_t op,list_t* reg_table,char** regs){
  if(!op.value)return NULL;
  switch (op.type) {
  case OPERAND_ADDRESS:return op.value;
  case OPERAND_IMMEDIATE:{
    size_t strl=strlen(op.value);
    char* fmt=malloc(strl+2);
    assert(fmt);
    memcpy(fmt+1,op.value,strl);
    op.value=fmt;
    op.value[0]='#';
    op.value[strl+1]='\0';
    return op.value;
  }
  case OPERAND_VALUE:{
    size_t strl=strlen(op.value);
    char* fmt=malloc(strl+3);
    assert(fmt);
    memcpy(fmt+1,op.value,strl);
    op.value=fmt;
    op.value[0]='[';
    op.value[strl+1]=']';
    op.value[strl+2]='\0';
    return op.value;
  }
  case OPERAND_TMPVAR: {
    for (size_t i=0; i < reg_table->len; ++i) {
      tmpvar_alloc_info_t* pair=list_get(reg_table, i);
      if (pair->tmpv_index && op.tmpvalue.index == pair->tmpv_index) {
	return regs[pair->reg];
      }
    }
    cry_errorf(SENDER_ASMGEN, ((filepos_t){0,0}), "tmpvar used before alloced");
    break;
  }
  default:break;
  }
  return op.value;
}
void aarch64_translate(list_t *list_asm, intercode_t *intercode,
                       size_t *stack_subbase, list_t *reg_table,
                       platform_info_t arch) {
  abi_t abi=get_abi(arch.abi);
  char* op1=format_operand(intercode->op1, reg_table, abi.caller_saved_regs);
  char* op2=format_operand(intercode->op2, reg_table, abi.caller_saved_regs);
  char* op3=format_operand(intercode->op3, reg_table, abi.caller_saved_regs);
  switch (intercode->type) {
  case CODE_DATA_SECTION:
    ASM(".data\n");
    break;
  case CODE_TEXT_SECTION:
    ASM(".text\n");
    break;
  case CODE_GLOBAL_VAR_DATA:
    ASM("%s: .dword %s\n",op1, op2);
    break;
  case CODE_DATA:{
    char* data=op2;
    char* width=".dword";
    if(data[0]=='\"'){
      width=".asciz";
    }
    ASM("%s: %s %s\n",op1, width, op2);
    break;
  }
  case CODE_DEF_FUNC: {
    ASM(".global %s\n",op1);
    ASM("%s:\n",op1);
    ASM("str fp,[sp,#-16]!\nmov fp,sp\n");    
    break;
  }
  case CODE_RETURN: {
    if(op1){
      ASM("mov x0,%s\n",op1);
    }
    ASM("mov sp,fp\nldr fp,[sp],#16\nret\n");
    break;
  }
  case CODE_ALLOC_GLOBAL:{
    char* initv=op2;
    if(!initv)initv="0";
    ASM("%s: .dword %s\n",op1,initv);
    //ASM(".set %s,[%s]\n",op1, op1);
    break;
  }
  case CODE_ALLOC_LOCAL: {
    int alloc_size=atoi(intercode->op2.value);
    if(alloc_size<16)alloc_size=16;
    ASM("sub sp,sp,#%d\n",alloc_size);
    size_t var_size=atoi(op2);
    *stack_subbase=*stack_subbase+var_size;
    ASM(".set %s,[fp,#-%zu]\n",op1,*stack_subbase);    
    break;
  }
  case CODE_ALLOC_TMP: {
    // todo: alloc registers accordingly
    tmpv_alloc_reg(reg_table, intercode->op1.tmpvalue,
                   abi.caller_saved_regs_num);    
    //ASM(".set %s,%s\n",op1,reg);
    break;
  }    
#define TWOOP(codenoprefix,ins)						\
    case CODE_##codenoprefix: {						\
      ASM("%s %s,%s,%s\n",						\
	  #ins,op3,op3,		\
    op2);						\
      break;								\
    }
    
    TWOOP(ADD, add);
    TWOOP(SUB, sub);
    TWOOP(BITAND, and);
    TWOOP(BITOR, or);
    TWOOP(BITNOT, not);    
    
  case CODE_MUL: {
    ASM("mul %s,%s,%s\n",op3, op1, op2);
    break;
  }
  case CODE_DIV: {
    ASM("udiv %s,%s,%s\n",op3, op1, op2);
    break;
  }
    
  case CODE_MOD: {
    //a%b=a-(a/b)*b
    ASM("str x0,[sp,#-16]!\nudiv x0,%s,%s\nmsub %s,x0,%s,%s\nldr x0,[sp],#16\n",
	op1,op2, op3,op2,op1);
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
    ASM(".extern %s\n",op1);
    break;
  }
#define JMP(casenoprefix,codenoprefix)					\
    case CODE_##casenoprefix: {					\
      ASM("%s %s\n",#codenoprefix,op1);	\
      break;							\
  }
    JMP(JE,beq);
    JMP(JNE,bne);
    JMP(JA,bgt);
    JMP(JB,blt);
    JMP(JAE,bge);
    JMP(JBE,ble);
    JMP(JMP,br);
  case CODE_LABEL: {
    ASM("%s:\n",op1);
    break;
  }
  case CODE_M2M:
    ASM("str x0,[sp,#-16]!\nldr x0,%s\nstr x0,%s\nldr x0,[sp],#16\n",
	op1, op2);
    break;
  case CODE_LOAD:
    ASM("ldr %s,%s\n",op1, op2);
    break;
  case CODE_STORE:
    ASM("str %s,%s\n",op2, op1);
    break;
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
    // todo: free tmpvars
    break;
  }
  case CODE_PUSHARG: {
    // push arg
    ASM("ldr %s,[sp,#-16]!\n",op1);
    stk_argsz+=16;
    break;
  }
  case CODE_FUNCCALL: {
    /* to pass args. we store the value of tmpvars into stack and then mov it to the registers before
     calling the func*/
    ASM("sub sp,sp,#64\n");
    ASM("stp x0,x1,[sp,#48]\n");
    ASM("stp x2,x3,[sp,#32]\n");
    ASM("stp x4,x5,[sp,#16]\n");
    ASM("stp x6,x7,[sp,#0]\n");
    char* regs[]={"x0","x1","x2","x3","x4","x5","x6","x7"};
    for (size_t i=0; i < stk_argsz/8; ++i) {
      // we copy the part that overflows registers reversely to meet system V
      // since the earlier pushargs are from left to right
      ASM("mov %s,[sp,#64]!\n",(i<8?regs[i]:"x0"));
      if(i>=8){
	ASM("str x0,[sp,#-16]!\n");
      }
    }
    ASM("bl %s\n",op1);
    if(stk_argsz>64){
      ASM("add sp,sp,#%zu\n",stk_argsz-64);
    }
    // restore the stacks used to pass args
    ASM("ldp x6,x7,[sp,#0]\n");
    ASM("ldp x4,x5,[sp,#16]\n");
    ASM("ldp x2,x3,[sp,#32]\n");
    ASM("ldp x0,x1,[sp,#48]\n");
    ASM("add sp,sp,#64\n");
    ASM("add sp,sp,#%zu\n",stk_argsz);
    stk_argsz=0;
    break;
  }
  case CODE_STORE_RETV: {
    ASM("mov %s,x0\n",op1);
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
  
char* aarch64_gen(list_t* intercodes,platform_info_t arch){
  list_t asm_codes=create_list(20, sizeof(char*));
  size_t stk=0;
  list_t reg_table=create_list(8, sizeof(tmpvar_alloc_info_t));
  // something
  ASMU(&asm_codes, ".arch arm64\n");
  
  for (size_t i=0; i < intercodes->len; ++i) {
    aarch64_translate(&asm_codes, list_get(intercodes, i),&stk,&reg_table,arch);    
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

