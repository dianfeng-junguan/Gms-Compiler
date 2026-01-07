#include "utils.h"
typedef enum {
  NODE_NONE=0,
  // this is usally used when a node has more than 2 subnodes.
  // we put other subnodes under a new leafholder which is under the real parent.
  NODE_LEAFHOLDER,
  // values
  NODE_CONSTANT,
  NODE_IDENTIFIER,
  // statements
  NODE_IF,
  NODE_ELSEIF,
  NODE_ELSE,
  NODE_WHILE,
  NODE_RETURN,

  NODE_FUNCTION,
  NODE_DEFINITION,
  // expressions
  NODE_COMMALIST,
  //operator
  NODE_ADD,
  NODE_SUB,
  NODE_MUL,
  NODE_DIV,
  NODE_MOD,
  NODE_BITAND,
  NODE_BITOR,
  NODE_XOR,
  //comparator
  NODE_EQUAL,
  NODEGREATER,
  NODE_LESS,
  NODE_GREATER_EQUAL,
  NODE_LESS_EQUAL,
  NODE_NOT_EQUAL,
  // action
  NODE_ASSIGN,
  //logic
  NODE_AND,
  NODE_OR,
  NODE_NOT,
}astnode_type_t;
typedef struct _astnode_t{
  astnode_type_t node_type;
  struct _astnode_t* left;
  struct _astnode_t* right;
  // used for value nodes.
  char* value;
} astnode_t;

astnode_t do_parse(list_t* tokens);

