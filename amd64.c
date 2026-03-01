#include "asmgen.h"
#include "err.h"
#include "utils.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "intercode.h"
#define ASM(fmt, ...)				\
  do {						\
    char *line = malloc(64);			\
    assert(line);				\
    snprintf(line, 64, fmt, ##__VA_ARGS__);	\
    list_append(list_asm, &line);		\
  } while (0);
static size_t stk_argsz = 0;
static char* format_operand(operand_t op,list_t* tmpv_table, char** regs, stackframe_t* sf){
  switch (op.type) {
  case OPERAND_TMPVAR: {
    for (size_t i=0; i < tmpv_table->len; ++i) {
      tmpvar_alloc_info_t *pair = list_get(tmpv_table, i);
      if (pair->tmpv_index != TMPVAR_INDEX_NULL && op.tmpvalue.index == pair->tmpv_index) {
        if (pair->onstack) {
          char tmpvname[20];
          sprintf(tmpvname, "tmp@%d", pair->tmpv_index);
          size_t offset = get_local_offset(sf, tmpvname);
	  cstring_t cstr = create_string();
	  string_sprintf(&cstr, "[rbp-%zu]", offset);
	  return cstr.data;
        } else {
	  return regs[pair->reg];
	}
      }
    }
    //cry_errorf(SENDER_ASMGEN, ((filepos_t){0,0}), "tmpvar used before alloced");
    break;
  }
  case OPERAND_VALUE:{
    size_t off = get_local_offset(sf, op.value);
    cstring_t cstr = create_string();
    string_sprintf(&cstr, "[rbp-%zu]", off);
    return cstr.data;
  }    
  case OPERAND_OFFSET: {
    for (size_t i=0; i < tmpv_table->len; ++i) {
      tmpvar_alloc_info_t* pair=list_get(tmpv_table, i);
      if (pair->tmpv_index > 0 && op.tmpvalue.index == pair->tmpv_index) {
	cstring_t cstr = create_string();
        if (pair->onstack) {
	  string_sprintf(&cstr, "[rbp-%zu+%zu]",pair->offset,op.offset);
	}else{
	  string_sprintf(&cstr, "[%s+%zu]",regs[pair->reg],op.offset);
	}
	return cstr.data;
      }
    }
    break;
  }
  default:break;
  }
  return op.value;
}
void amd64_translate(list_t *list_asm, list_t *ics, list_t *tmpvar_table,
                     platform_info_t arch) {  
  size_t iter = 0;
  stackframe_t current_sf = create_stackframe();
  abi_t abi = get_abi(arch.abi);  
  while (iter < ics->len) {
    intercode_t *intercode=list_get(ics, iter++);
    char* op1=format_operand(intercode->op1, tmpvar_table,abi.caller_saved_regs,&current_sf);
    char* op2=format_operand(intercode->op2, tmpvar_table,abi.caller_saved_regs,&current_sf);
    char *op3 = format_operand(intercode->op3, tmpvar_table,
                               abi.caller_saved_regs, &current_sf);
    char icstr[64];
    intercode_tostr(icstr, intercode);    
    ASM("; %s\n",icstr);
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
	ASM("push %s\n", abi.callee_saved_regs[i]);
	grow_stack(&current_sf, 8);
      }
      // collect ALLOC_LOCALs
      size_t i = iter;
      size_t local_tot_sz = 0;
      while (i < ics->len) {
        intercode_t *code = list_get(ics, i);
	if (code->type==CODE_DEF_FUNC_END) {
	  break;
        } else if (code->type == CODE_ALLOC_LOCAL) {
	  size_t sz=atoi(code->op2.value);
          add_local(&current_sf, code->op1.value, sz);
          local_tot_sz += sz;          
	}
	i++;
      }
      // alloc stack for locals      
      ASM("sub rsp, %zu\n",local_tot_sz);
      break;
    }
    case CODE_RETURN: {
      if(op1){
	ASM("mov rax,%s\n",op1);
      }
      for (size_t i = 0; i < abi.callee_saved_regs_num; i++) {
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
      //ASM("sub rsp,%s\n",op2);
      //size_t var_size=atoi(op2);
      //*stack_subbase=*stack_subbase+var_size;
      //size_t offset=add_local(&current_sf, op1, var_size);
      //ASM("%%define %s qword [rbp-%zu]\n", op1, offset);    
      break;
    }
    case CODE_ALLOC_TMP: {
      size_t tmpsz = intercode->op1.tmpvalue.size;
      int r=0;
      if (tmpsz<=8) {
	r = tmpv_alloc_reg(tmpvar_table, intercode->op1.tmpvalue, abi.callee_saved_regs_num);      
      }
      if (r == -1 || tmpsz > 8) {
	// need to alloc stack space
	tmpv_alloc_stack(tmpvar_table, intercode->op1.tmpvalue, &current_sf);
	ASM("sub rsp,%zu\n",intercode->op1.tmpvalue.size);
      }
    
      //ASM("%%define %s %s\n",op1,reg);
      break;
    }    
#define TWOOP(codenoprefix,ins)			\
      case CODE_##codenoprefix: {		\
	ASM("mov %s,%s\n%s %s,%s\n",op3,op1,	\
	    #ins,op3,op2);			\
	break;					\
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
#define JMP(codenoprefix)			\
      case CODE_##codenoprefix: {		\
	ASM("%s near .%s\n",#codenoprefix,op1);	\
	break;					\
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
      // check if the size of operands exceed the register size
      operand_t *sop1 = &intercode->op1;
      operand_t *sop2 = &intercode->op2;
      // intercoder needs to make sure such large MOVs has at least one tmpvar      
      if (sop1->type == OPERAND_TMPVAR && sop1->tmpvalue.size > 8 ||
          sop2->type == OPERAND_TMPVAR && sop2->tmpvalue.size > 8) {
        // this is a large MOV. break it down to smaller ones
        size_t objsz = sop1->type==OPERAND_TMPVAR?sop1->tmpvalue.size:sop2->tmpvalue.size;
        // the two operands must be in the stack
        cstring_t op1name = sop1->value?string_from(sop1->value):create_string();
        cstring_t op2name = sop2->value?string_from(sop2->value):create_string();
        if (sop1->type==OPERAND_TMPVAR) {
          string_sprintf(&op1name, "tmp@%d", sop1->tmpvalue.index);
        }
        if (sop2->type == OPERAND_TMPVAR) {          
          string_sprintf(&op2name, "tmp@%d", sop2->tmpvalue.index);
	}
        size_t offset1 = get_local_offset(&current_sf, op1name.data);
        size_t offset2 = get_local_offset(&current_sf, op2name.data);
        ASM("push rsi\npush rdi\npush rcx\n");
        ASM("lea rsi,[rbp-%zu]\nlea rdi,[rbp-%zu]\n", offset2, offset1);
        ASM("mov rcx,%zu\n", objsz);        
	ASM("rep movsb\n");
        ASM("pop rcx\npop rdi\npop rsi\\n");
	break;        
      }
      // check if both operands are mems (invalid)
      if (intercode->op1.type==OPERAND_VALUE&&intercode->op2.type==OPERAND_VALUE) {
	ASM("push rax\nmov rax,%s\nmov %s,rax\npop rax\n",op2,op1);
      }else{
	ASM("mov %s,%s\n",op1, op2);
      }
      break;
    }
    case CODE_SCOPE_END: {
      // actually we do not really need to free the stack alloced by the deeper scope.
      // when func returns, they will all be freed.
      break;
    }
    
    case CODE_DEF_FUNC_END: {
      // clear the stackframe    
      clear_stack(&current_sf);
      break;
    }
    case CODE_FREE: {
      // useful for tempvars
      free_tmpvar(tmpvar_table, intercode->op1.tmpvalue,&current_sf);
      
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
	if (is_reg_used(tmpvar_table, i) &&
	    strcmp(ret_reg, abi.caller_saved_regs[i]) != 0) {        
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
	if (is_reg_used(tmpvar_table, i - 1) &&
	    strcmp(ret_reg, abi.caller_saved_regs[i - 1]) != 0) {        
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
      panic("fatal: met unrecognized intercode %s.",
            codetype_tostr(intercode->type));      
      break;
    }
  }
  free_stackframe(&current_sf);
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
  list_t tmpvar_table = create_list(6, sizeof(tmpvar_alloc_info_t));
  abi_t abi = get_abi(arch.abi);
  // something
  ASMU(&asm_codes, "bits 64\n");
  ASMU(&asm_codes, "default rel\n");
  
  amd64_translate(&asm_codes, intercodes,&tmpvar_table,arch);
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
  FREE_LIST_DTOR(&tmpvar_table, free_reg_str_pair);
  return result;
}
