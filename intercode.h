#include "utils.h"
#include <stddef.h>
typedef enum {
  CODE_DEF_FUNC,
  CODE_DEF_FUNC_END,
  CODE_SCOPE_END,
  CODE_DECLARE,
  CODE_EXTERN_DECLARE,
  CODE_ALLOC_GLOBAL,
  CODE_ALLOC_LOCAL,
  CODE_ALLOC_TMP,
  CODE_FREE,
  CODE_RETURN,
  CODE_MOV,

  // operators
  CODE_ADD,
  CODE_SUB,
  CODE_MUL,
  CODE_DIV,
  CODE_MOD,
  CODE_REFER,
  CODE_DEFER,
  CODE_BITAND,
  CODE_BITOR,
  CODE_BITNOT,
  // compare
  CODE_CMP,
  // jmping
  CODE_JMP,
  CODE_JA,
  CODE_JAE,
  CODE_JB,
  CODE_JBE,
  CODE_JE,
  CODE_JNE,
  CODE_LABEL,

  CODE_PUSHARG,
  CODE_FUNCCALL,
  CODE_STORE_RETV,
  // second-time processing intercodes
  // indicate asmgen to put constant at data section
  // CODE_DATA name data
  CODE_DATA,
  // mem->tmpvar  
  CODE_LOAD,
  // tmpvar->mem
  CODE_STORE,
  //mem->mem  
  CODE_M2M,
  // CODE_MOV became tmpvar->tmpvar
  // used to alloc a mem for global vars
  // this is differente from CODE_DATA. when following codes used CODE_DATA alloced name,
  // the asmgen will replace it with the address(or label representing the address) of the data;
  // as for CODE_GLOBAL_VAR_DATA the asmgen will operate with the value rather than the address.
  // it is a difference like var and [var] in nasm.
  // CODE_GLOBAL_VAR_DATA name data
  CODE_GLOBAL_VAR_DATA,
  CODE_DATA_SECTION,
  CODE_TEXT_SECTION,
} intercode_type_t;

typedef enum {
  OPERAND_EMPTY = 0,
  OPERAND_IMMEDIATE,
  OPERAND_STRING,
  OPERAND_TMPVAR,
  OPERAND_VALUE,
  OPERAND_ADDRESS,
  OPERAND_KEEP,
  OPERAND_OFFSET,
  OPERAND_OFFSET_VALUE_TMP,
} operandtype_t;

typedef struct _tmpvar_t{
  // index used to mark the tmpvar
  int index;
  size_t size;
}tmpvar_t;
typedef struct {
  operandtype_t type;
  union {
    // sym name
    char *value;
    // numerical value (usually immediate)
    long long num_value;
    // tmpvar    
    tmpvar_t tmpvalue;
  };
  union{
    size_t offset;
    tmpvar_t offsettmp;
  };
    
}operand_t;
typedef struct {
  intercode_type_t type;
  operand_t op1,op2,op3;
}intercode_t;

typedef struct _astnode_t astnode_t;
list_t gen_intercode(astnode_t *ast);
void free_intercode(intercode_t* code);
void push_code(list_t *code_list, intercode_type_t code_type, operand_t op1,
               operand_t op2, operand_t op3);
void intercode_tostr(char* buf, intercode_t *ic);
const char *codetype_tostr(intercode_type_t type);
char* codeop_fmt(operand_t op);
// the operand is an immediate value.
#define IMM(immediate)                                                         \
  ((operand_t){.type = OPERAND_IMMEDIATE, .num_value = immediate})
/// create an immediate operand from string
operand_t imm_str(char* strv);
// the operand is a tmpvar. this usually means it will be implemented by registers.
#define TMP(tmpvar) ((operand_t){.type = OPERAND_TMPVAR, .tmpvalue = tmpvar})
// the operand is stored in a mem area. this could be variables or something
// which you wanna take value of.
// this will be interpreted like this in nasm:
// mov rax,[value]
#define VALUE(v) ((operand_t){.type = OPERAND_VALUE, .value = (v)})
// the operand calculates the address by offseting the address of tmp by off and take the value as the operand
#define OFFSETTMP(tmp, off)                                                    \
  ((operand_t){.type = OPERAND_OFFSET, .tmpvalue = (tmp), .offset = off})
// the operand use a symbol as base address and the value of tmpvar as offset
#define OFFSETVT(v, offtmp) ((operand_t){.type= OPERAND_OFFSET_VALUE_TMP, .value=(v), .offsettmp=(offtmp)})
// the operand is the pointer to a certain mem area. it uses the address of the
// passed argument. this is used to jump to the address
// in assembler, such operand will be interpreted as like this in nasm:
// mov rax,address
#define ADDR(address) ((operand_t){.type = OPERAND_ADDRESS, .value = (address)})
#define STRCONST(str) ((operand_t){.type= OPERAND_STRING, .value=str})
// the operand is directly pasted to the assembly code.
#define KEEP(content) ((operand_t){.type=OPERAND_KEEP,.value=(content)})
#define EMPTY ((operand_t){0})
intercode_t create_code(intercode_type_t type, operand_t operand1, operand_t operand2, operand_t operand3);
#define CODE(code_list, type, op1, op2, op3)                                   \
  do {                                                                         \
    intercode_t __code = create_code(type, op1, op2, op3);                     \
    append(code_list, &__code);                                            \
  } while (0);

#define TMPV_EMPTY(tmpvart) (tmpvart.index!=-1)
#define TMPVAR_INDEX_NULL -1
