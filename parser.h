#include "utils.h"
typedef enum {
  NODE_NONE = 0,
  // this is usally used when a node has more than 2 subnodes.
  // we put other subnodes under a new leafholder which is under the real
  // parent.
  NODE_LEAFHOLDER,
  NODE_SINGLEEXPR,
  // values
  NODE_CONSTANT,
  NODE_IDENTIFIER,
  // statements
  NODE_IF,
  NODE_ELSEIF,
  NODE_ELSE,
  NODE_WHILE,
  NODE_RETURN,
  NODE_BREAK,

  NODE_FUNCTION,
  NODE_DEFINITION,
  NODE_EXTERN,
  NODE_DECLARE_VAR,
  NODE_DECLARE_FUNC,
  // expressions
  NODE_COMMALIST,
  // operator
  NODE_ADD,
  NODE_SUB,
  NODE_MUL,
  NODE_DIV,
  NODE_MOD,
  NODE_REFER,
  NODE_DEFER,
  NODE_BITAND,
  NODE_BITOR,
  NODE_XOR,
  // comparator
  NODE_EQUAL,
  NODE_GREATER,
  NODE_LESS,
  NODE_GREATER_EQUAL,
  NODE_LESS_EQUAL,
  NODE_NOT_EQUAL,
  // action
  NODE_ASSIGN,
  // logic
  NODE_AND,
  NODE_OR,
  NODE_NOT,
  //
  NODE_FUNCCALL,
  // others
  NODE_ARGLIST,
  NODE_ARGPAIR,
  NODE_TYPEKW
} astnode_type_t;

typedef enum {
  TYPE_VOID = 0,
  TYPE_INT,
  TYPE_STRING,
  TYPE_POINTER, // not allowed in minor type
} symbol_single_type_t;
typedef struct {
  symbol_single_type_t main_type;
  // if the main type is pointer, it points to a var of minor type
  symbol_single_type_t minor_type;
  // e.g. 2-level pointer int** 
  int pointer_level;
} symbol_type_t;
int symtypcmp(symbol_type_t a, symbol_type_t b);

typedef struct _astnode_t{
  astnode_type_t node_type;
  struct _astnode_t* left;
  struct _astnode_t* right;
  // used for value nodes.
  char* value;
  // used for sematics.
  list_t syms;
  filepos_t position;
  // layer here means the depth of the scope.
  // used to differentiate symbols defined at different depths of scopes.
  int layer;
  // used to indicate the type of constant or expression
  symbol_type_t value_type;
} astnode_t;
typedef enum {
  SYMBOL_VARIABLE,
  SYMBOL_FUNCTION,
} symbol_kind_t;
typedef struct{
  char* name;
  /// this indicates whether it is a variable or function
  symbol_kind_t type;
  
  union{
    symbol_type_t sym_type;
    symbol_type_t return_type;
  };
  int layer;
  unsigned long long value;
  int is_extern;
  // function args. the elements are of type symbol_type_t
  list_t args;
}symbol_t;
astnode_t *do_parse(list_t *tokens);

char* get_nodetype_str(astnode_type_t type);

void free_node(astnode_t *node);
void free_symbol(symbol_t *sym);
void free_all_nodes();
