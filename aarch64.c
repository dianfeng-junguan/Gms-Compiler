#include "asmgen.h"
#include "err.h"
#include "utils.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "intercode.h"
static size_t stk_argsz = 0;
static int tmplabelid=0;
static char* format_operand(operand_t op,list_t* tmpv_table, char** regs, stackframe_t* sf){
  switch (op.type) {
  case OPERAND_TMPVAR: {
    for (size_t i=0; i < tmpv_table->len; ++i) {
      tmpvar_alloc_info_t *pair = list_get(tmpv_table, i);
      if (pair->tmpv_index != TMPVAR_INDEX_NULL && op.tmpvalue.index == pair->tmpv_index) {
        if (pair->onstack) {
          size_t offset = stackframe_get_tmpvar_offset(sf, op.tmpvalue.index);
	  cstring_t cstr = create_string();
	  string_sprintf(&cstr, "[fp,#-0x%lx]", offset);
	  return cstr.data;
        } else {
	  return regs[pair->reg];
	}
      }
    }
    //cry_errorf(SENDER_ASMGEN, ((filepos_t){0,0}), "tmpvar used before alloced");
    break;
  }
  case OPERAND_STRING: {
    return op.value;
  }
  case OPERAND_IMMEDIATE: {
    cstring_t cstr = create_string();      
    string_sprintf(&cstr, "#%d", op.num_value);
    return cstr.data;
    break;
  }
  case OPERAND_VALUE:{
    size_t off = get_local_offset(sf, op.value);
    cstring_t cstr = create_string();
    string_sprintf(&cstr, "[fp,#-0x%lx]", off);
    return cstr.data;
  }    
  case OPERAND_OFFSET: {
    for (size_t i=0; i < tmpv_table->len; ++i) {
      tmpvar_alloc_info_t* pair=list_get(tmpv_table, i);
      if (pair->tmpv_index > 0 && op.tmpvalue.index == pair->tmpv_index) {
	cstring_t cstr = create_string();
        if (pair->onstack) {
	  string_sprintf(&cstr, "[fp,#-0x%lx]",pair->offset-op.offset);
	}else{
	  string_sprintf(&cstr, "[%s,#0x%lx]",regs[pair->reg],op.offset);
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
/*
  push an aarch64 mov instruction to the asm code list.
  it automatically select the correct instruction for different operands whose
  size is no larger than a register.
  for large MOVs (operands which is too large to be held by a register), do not
  use this function.
 */
void aarch64_mov(list_t *list_asm, operand_t op1, operand_t op2,
                 char *formatted_op1, char *formatted_op2, stackframe_t *sf,
                 list_t *tmpv_table, char** regs
                 ) {
#define OPERAND_ISMEM(operand) (operand.type==OPERAND_VALUE||operand.type==OPERAND_OFFSET)  
  // check if both operands are mems (invalid)
  if (OPERAND_ISMEM(op1)&&OPERAND_ISMEM(op2)) {
    ASM("str x0,[sp,#-16]!\nldr x0,%s\nstr x0,%s\nldr x0,[sp],#16\n",
        formatted_op2, formatted_op1);
  } else if (op1.type == OPERAND_TMPVAR && OPERAND_ISMEM(op2)) {
    long long local_offset = op2.type == OPERAND_VALUE
                                 ? get_local_offset(sf, op2.value)
                                 : op2.offset;
    if(local_offset == -1l){
      panic("asm generator cannot find offset of operand type=%d, tmpv index=%d , value=%s\n",op2.type,op2.tmpvalue.index,op2.value);
    }
    if (local_offset>255l||local_offset<-255l) {
      // use a register to temporarily store the address 
      char *tmpreg = "x0";
      if (strcmp(tmpreg, formatted_op1)==0) {
	tmpreg = "x1";
      }
      // just use the op1 register as it will be overwritten by ldr-ed value
      // anyway
      if (op2.type==OPERAND_OFFSET) {
        // op2 is register offset
	// get the base register        
	tmpvar_alloc_info_t* tinfo=get_tmpv_alloc(tmpv_table, op2.tmpvalue.index);
        tmpreg = regs[tinfo->reg];
        ASM("add %s,%s,#0x%llx\n", formatted_op1, tmpreg, local_offset);
      } else {
	// OPERAND_VALUE, op2 is fp offset        
	ASM("sub %s,fp,#0x%llx\n", formatted_op1, local_offset);
      }
      ASM("ldr %s,[%s]\n", formatted_op1, tmpreg);
    }else{
      ASM("ldr %s,%s\n",formatted_op1,formatted_op2);
    }
  } else if (OPERAND_ISMEM(op1) && op2.type == OPERAND_TMPVAR) {
  long long local_offset = op1.type == OPERAND_VALUE
    ? get_local_offset(sf, op1.value)
    : op1.offset;
  assert(local_offset != -1l);
  if (local_offset>255l||local_offset<-255l) {
    // use a register to temporarily store the address 
    char *tmpreg = "x0";
    if (strcmp(tmpreg, formatted_op2)==0) {
      tmpreg = "x1";
    }
    ASM("str %s,[sp,#-16]!\n",tmpreg);
    if (op1.type==OPERAND_OFFSET) {
      // op1 is register offset
      // get the base register        
      tmpvar_alloc_info_t* tinfo=get_tmpv_alloc(tmpv_table, op1.tmpvalue.index);
      // store the offset result into tmpreg
      ASM("add %s,%s,#0x%llx\n", tmpreg, regs[tinfo->reg], local_offset);
    } else {
      // OPERAND_VALUE, op1 is fp offset        
      ASM("sub %s,fp,#0x%llx\n", tmpreg, local_offset);
    }
    ASM("str %s,[%s]\n", formatted_op2, tmpreg);
    ASM("ldr %s,[sp],#16\n",tmpreg);
  }else{
    ASM("str %s,%s\n",formatted_op2,formatted_op1);
  }      
  }else if(op2.type==OPERAND_ADDRESS){
    ASM("mov %s,%s\n",formatted_op1,formatted_op2);
  }else if(op2.type==OPERAND_STRING){
    // string constant operands are converted to labels
    ASM("adrp %s,%s@PAGE\n", formatted_op1, formatted_op2);
    ASM("add %s,%s,%s@PAGEOFF\n", formatted_op1, formatted_op1, formatted_op2);    
  }else{
    ASM("mov %s,%s\n",formatted_op1,formatted_op2);
  }
  
}
void aarch64_translate(list_t *list_asm, list_t *ics, list_t *tmpvar_table,
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
      ASM(".data\n");
      break;
    case CODE_TEXT_SECTION:
      ASM(".text\n");
      break;
    case CODE_GLOBAL_VAR_DATA:
      ASM("%s: .dword %s\n.set %s .dword %s\n",op1, op2,
	  op1, op1);
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
      // we don't have to store callee_saved_regs now because we just won't
      // use them at present
      /*char *r1,*r2;
      for (size_t i = 0; i < abi.callee_saved_regs_num; i++) {
        if (i % 2 == 0) {
          if (i>0) {
            ASM("stp %s,%s,[sp,#-16]!\n", r1, r2);
            grow_stack(&current_sf, 16);
          }
          r1=abi.callee_saved_regs[i];
        }else{
          r2=abi.callee_saved_regs[i];
        }

      }
      if (abi.callee_saved_regs_num%2!=0) {
        ASM("str %s,[sp,#-16]!\n", r1);
        grow_stack(&current_sf, 8);
      }
      */
      // collect ALLOC_LOCALs
      size_t i = iter;
      size_t local_tot_sz = 0;
      while (i < ics->len) {
        intercode_t *code = list_get(ics, i);
	if (code->type==CODE_DEF_FUNC_END) {
	  break;
        } else if (code->type == CODE_ALLOC_LOCAL) {
	  size_t sz=code->op2.num_value;
          size_t offset = add_local(&current_sf, code->op1.value, sz);
          do_log(VERBOSE, ASMGEN_ALLOCLOCAL, "variable %s at [fp-0x%lx] size 0x%lx\n",
                 code->op1.value, offset,sz);
          local_tot_sz += sz;
	}
	i++;
      }
      // align up tot_sz to multiplies of 16
      size_t aligned_local_tot_sz = (local_tot_sz + 16) & ~15;
      size_t delta = aligned_local_tot_sz - local_tot_sz;
      grow_stack(&current_sf, delta);
      // alloc stack for locals      
      ASM("sub sp,sp,#0x%lx\n",aligned_local_tot_sz);
      break;
    }
    case CODE_RETURN: {
      if (intercode->op1.type!=OPERAND_EMPTY) {
	if (intercode->op1.type==OPERAND_TMPVAR) {
	  ASM("mov x0,%s\n",op1);
	}else{
	  ASM("ldr x0,%s\n",op1);
	}
      }
      /*char *r1,*r2;
      if (abi.callee_saved_regs_num%2!=0) {
	ASM("ldr %s,[sp],#16\n",abi.callee_saved_regs[abi.callee_saved_regs_num-1]);
      }
      for (size_t i = 0; i < abi.callee_saved_regs_num-1; i++) {
	if (i%2==0) {
          r1 = abi.callee_saved_regs[abi.callee_saved_regs_num - 2 - i];
	  if (i>0) {
	    ASM("ldp %s,%s,[sp],#16\n",r2,r1);
	  }
	}else{
	  r2=abi.callee_saved_regs[abi.callee_saved_regs_num-2-i];
	}
      }*/
      ASM("mov sp,fp\nldr fp,[sp],#16\nret\n");
      break;
    }
    case CODE_ALLOC_GLOBAL:{
      char* initv=op2;
      if(!initv)initv="0";
      ASM("%s: .dword %s\n",op1,initv);
      ASM(".set %s .dword [%s]\n",op1, op1);
      break;
    }
    case CODE_ALLOC_LOCAL: {
      //ASM("sub sp,%s\n",op2);
      //size_t var_size=atoi(op2);
      //*stack_subbase=*stack_subbase+var_size;
      //size_t offset=add_local(&current_sf, op1, var_size);
      //ASM(".set %s .dword [fp-0x%lx]\n", op1, offset);    
      break;
    }
    case CODE_ALLOC_TMP: {
      size_t tmpsz = intercode->op1.tmpvalue.size;
      
      int r=0;
      if (tmpsz<=8) {
	r = tmpv_alloc_reg(tmpvar_table, intercode->op1.tmpvalue, abi.callee_saved_regs_num);      
      }
      if (r == -1 || tmpsz > 8) {
	intercode->op1.tmpvalue.size=ALIGNUP(intercode->op1.tmpvalue.size, 16);
	// need to alloc stack space
	tmpv_alloc_stack(tmpvar_table, intercode->op1.tmpvalue, &current_sf);
	ASM("sub sp,sp,#0x%lx\n",intercode->op1.tmpvalue.size);
      }
    
      //ASM(".set %s %s\n",op1,reg);
      break;
    }    
#define TWOOP(codenoprefix,ins)			\
      case CODE_##codenoprefix: {		\
	ASM("%s %s,%s,%s\n",#ins,op3,		\
	    op1,op2);				\
	break;					\
      }
    
      TWOOP(ADD, add);
      TWOOP(SUB, sub);
      TWOOP(MUL, mul);
      TWOOP(DIV, udiv);
      TWOOP(BITAND, and);
      TWOOP(BITOR, orr);
    case CODE_BITNOT: {
      ASM("mvn %s,%s\n",op1,op2);
      break;
    }
    case CODE_REFER: {
      switch (intercode->op2.type) {
      case OPERAND_TMPVAR:
      case OPERAND_OFFSET: {
	// a large tmpvar which is stored in the stack
        long long offset = stackframe_get_tmpvar_offset(&current_sf, intercode->op2.tmpvalue.index);
        assert(offset != -1);
	ASM("sub %s,fp,#%llu\n",op1,offset);
        break;
      }        
      case OPERAND_VALUE:{
        long long offset = get_local_offset(&current_sf, intercode->op2.value);
        assert(offset != -1);
	ASM("sub %s,fp,#%llu\n",op1,offset);
        break;
      }
      default:
	ASM("mov %s,%s\n",op1,op2);        
	break;
      }
      break;
    }
    case CODE_DEFER: {
      if (intercode->op2.type==OPERAND_VALUE) {
	ASM("sub %s,fp,#0x%lx\n",op1,(size_t)get_local_offset(&current_sf, intercode->op2.value));
      } else {
	ASM("mov %s,%s\nldr %s,[%s]\n",op1,op2,op1,op1);
      }
      break;
    }

    case CODE_MOD: {
      // op3=op1%op2=op1-(op1/op2)*op2
      //a%b=a-(a/b)*b
      // a/b
      ASM("udiv %s,%s,%s\n", op3, op1, op2);
      // a-(a/b)*b
      ASM("msub %s,%s,%s,%s\n", op3, op3, op2, op1);      
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
#define JMP(codenoprefix,ins)		\
      case CODE_##codenoprefix: {	\
	ASM("%s %s\n",#ins,op1);	\
	break;				\
      }
      JMP(JE,beq);
      JMP(JNE,bne);
      JMP(JA,bgt);
      JMP(JB,blt);
      JMP(JAE,bge);
      JMP(JBE,ble);
      JMP(JMP,b);
    case CODE_LABEL: {
      ASM("%s:\n",op1);
      break;
    }
    case CODE_M2M:
    case CODE_LOAD: 
    case CODE_STORE:
    case CODE_MOV: {
      // check if the size of operands exceed the register size
      operand_t *sop1 = &intercode->op1;
      operand_t *sop2 = &intercode->op2;
      // intercoder needs to make sure such large MOVs has at least one tmpvar      
      if ((sop1->type == OPERAND_TMPVAR && sop1->tmpvalue.size > 8) ||
          (sop2->type == OPERAND_TMPVAR && sop2->tmpvalue.size > 8)) {
        // this is a large MOV. break it down to smaller ones
        size_t objsz = sop1->type==OPERAND_TMPVAR?sop1->tmpvalue.size:sop2->tmpvalue.size;
        // the two operands must be in the stack
	long long offset1 = -1;        
        long long offset2 = -1;
	if (sop1->type==OPERAND_TMPVAR) {
	  offset1 = stackframe_get_tmpvar_offset(&current_sf, sop1->tmpvalue.index);
	}else{
	  offset1 = get_local_offset(&current_sf, sop1->value);
	}
        if (sop2->type==OPERAND_TMPVAR) {
	  offset2 = stackframe_get_tmpvar_offset(&current_sf, sop2->tmpvalue.index);
	}else{
	  offset2 = get_local_offset(&current_sf, sop2->value);
	}
	assert(offset1!=-1l&&offset2!=-1l);
        
        ASM("str x1,[sp,#-16]!\nstr x2,[sp,#-16]!\n");
        ASM("str x3,[sp,#-16]!\nstr x4,[sp,#-16]!\n");
        ASM("sub x2,fp,#0x%lx\nsub x3,fp,#0x%lx\n", (size_t)offset2, (size_t)offset1);
        ASM("mov x4,#0x%lx\n", objsz);
        ASM("tmplabel_memcpy%d:\n", tmplabelid++);
        ASM("ldrb w1,[x2]\nstrb w1,[x3]\n");
        ASM("add x2,x2,#1\nadd x3,x3,#1\n");
        ASM("sub x4,x4,#1\n");
	ASM("cbnz x4,tmplabel_memcpy%d\n",tmplabelid-1);
        ASM("ldr x4,[sp],#16\nldr x3,[sp],#16\n");
        ASM("ldr x2,[sp],#16\nldr x1,[sp],#16\n");
	break;
      }
      aarch64_mov(list_asm, intercode->op1, intercode->op2, op1, op2, &current_sf, tmpvar_table, abi.caller_saved_regs);
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
      // str arg,[sp,#-16]!
      ASM("str %s,[sp,#-16]!\n",op1);
      stk_argsz+=16;
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
	  ASM("str %s,[sp,#-16]!\n",abi.caller_saved_regs[i]);
	  pushed_regs_num++;
	}
      }
      for (size_t i=0; i < stk_argsz/16; ++i) {
	// we copy the part that overflows registers reversely
	// since the earlier pushargs are from left to right
	ASM("ldr %s,[sp,#0x%lx]\n",(i<abi.arg_regs_num?abi.arg_regs[i]:"x0"),16*pushed_regs_num+i*16);
	if(i>=abi.arg_regs_num){
	  // str the,[sp,#-16]! arg when num of args is more than arg registers
	  ASM("str x0,[sp,#-16]!\n");
	}
      }
      ASM("bl %s\n",op1);
      if(stk_argsz>16*abi.arg_regs_num){
	ASM("add sp,sp,#0x%lx\n",stk_argsz-16*abi.arg_regs_num);
      }
      if(op2){
        // store the return value
	if (intercode->op2.type==OPERAND_VALUE) {
	  ASM("str x0,%s\n",op2);
	}else {
	  ASM("mov %s,x0\n",op2);
	}
      }
      // restore the stacks used to pass args
      for (size_t i=abi.caller_saved_regs_num; i>0; i--) {
	if (is_reg_used(tmpvar_table, i - 1) &&
	    strcmp(ret_reg, abi.caller_saved_regs[i - 1]) != 0) {        
	  ASM("ldr %s,[sp],#16\n",abi.caller_saved_regs[i-1]);
	}
      }
      ASM("add sp,sp,#0x%lx\n",stk_argsz);
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
char* aarch64_gen(list_t* intercodes,platform_info_t arch){
  list_t asm_codes=create_list(20, sizeof(char*));
  list_t tmpvar_table = create_list(6, sizeof(tmpvar_alloc_info_t));
  abi_t abi = get_abi(arch.abi);
  // something
  // ASMU(&asm_codes, ".arch arm64\n");
  
  aarch64_translate(&asm_codes, intercodes,&tmpvar_table,arch);
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
