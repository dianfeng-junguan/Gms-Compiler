#include "utils.h"
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
  CODE_LOAD,// mem->tmpvar
  CODE_STORE,//tmpvar->mem
  CODE_M2M,//mem->mem
  // CODE_MOV became tmpvar->tmpvar
  // used to alloc a mem for global vars
  // this is differente from CODE_DATA. when following codes used CODE_DATA alloced name,
  // the asmgen will replace it with the address(or label representing the address) of the data;
  // as for CODE_GLOBAL_VAR_DATA, the asmgen will operate with the value rather than the address.
  // it is a difference like var and [var] in nasm.
  // CODE_GLOBAL_VAR_DATA name data
  CODE_GLOBAL_VAR_DATA,
  CODE_DATA_SECTION,
  CODE_TEXT_SECTION,
} intercode_type_t;

typedef enum{
  OPERAND_EMPTY=0,
  OPERAND_IMMEDIATE,
  OPERAND_TMPVAR,
  OPERAND_VALUE,
  OPERAND_ADDRESS,
  OPERAND_KEEP,
}operandtype_t;
typedef struct {
  operandtype_t type;
  char* value;
}operand_t;
typedef struct {
  intercode_type_t type;
  operand_t op1,op2,op3;
}intercode_t;

typedef struct _astnode_t astnode_t;
list_t gen_intercode(astnode_t *ast);
void free_intercode(intercode_t* code);
char *codetype_tostr(intercode_type_t type);
// the operand is an immediate value.
#define IMM(immediate)                                                         \
  ((operand_t){.type = OPERAND_IMMEDIATE, .value = immediate})
// the operand is a tmpvar. this usually means it will be implemented by registers.
#define TMP(tmpvar) ((operand_t){.type = OPERAND_TMPVAR, .value = tmpvar})
// the operand is stored in a mem area. this could be variables or something
// which you wanna take value of.
// this will be interpreted like this in nasm:
// mov rax,[value]
#define VALUE(v) ((operand_t){.type = OPERAND_VALUE, .value = (v)})
// the operand is the pointer to a certain mem area. it uses the address of the
// passed argument. this is used to jump to the address
// in assembler, such operand will be interpreted as like this in nasm:
// mov rax,address
#define ADDR(address) ((operand_t){.type = OPERAND_ADDRESS, .value = (address)})
// the operand is directly pasted to the assembly code.
#define KEEP(content) ((operand_t){.type=OPERAND_KEEP,.value=(content)})
#define EMPTY ((operand_t){0})
intercode_t create_code(intercode_type_t type, operand_t operand1, operand_t operand2, operand_t operand3);
#define CODE(code_list, type, op1, op2, op3)                                   \
  do {                                                                         \
    intercode_t __code = create_code(type, op1, op2, op3);                     \
    append(code_list, &__code);                                            \
  } while (0);

