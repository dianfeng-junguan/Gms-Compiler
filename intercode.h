#include "utils.h"
typedef enum {
  CODE_DEF_FUNC,
  CODE_DEF_FUNC_END,
  CODE_SCOPE_END,
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
  CODE_FUNCCALL
  //
} intercode_type_t;
typedef unsigned long long u64;
typedef struct {
  intercode_type_t type;
  union{
    char* label;
    char* varname;
    u64 operand1;
    char* operand1str;
  };
  union{
    u64 operand2;
    char* operand2str;
    u64 varsize;
  };
  union{
    u64 operand3;
    char* operand3str;
    u64 store_to;
    char* store_var;
  };
}intercode_t;

typedef struct _astnode_t astnode_t;
list_t gen_intercode(astnode_t* ast);
char *codetype_tostr(intercode_type_t type);
