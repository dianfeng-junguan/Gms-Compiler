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
  NODE_CLASS,
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
  // visit property of object

  NODE_PROPERTY,
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
  NODE_FUNCCALL,
  // others
  NODE_ARGLIST,
  NODE_ARGPAIR,
  NODE_KEYVALUE_PAIR,
  NODE_TYPEKW,
  NODE_CLASSMEMBER,
  // filling the class fields  
  NODE_CLASSFILL,
  
} astnode_type_t;

typedef enum {
  TYPE_VOID = 0,
  TYPE_INT,
  TYPE_STRING,
  TYPE_POINTER, // not allowed in minor type
} symbol_single_type_t;
typedef struct{
  char *name;
  symbol_type_index_t type;
}name_type_pair_t;

typedef struct {  
  // name of this type  
  char *name;
  // memsize it takes up. class will add up the sizes of the members but the
  // final size depends on the memory alignment chosen.  
  size_t size;
  union {
    // if this is a class, it records the members
    /*
      item struct:
      typedef struct{
      char *name;
      symbol_type_index_t type;
      }name_type_pair_t;
     */    
    list_t members;
    // if this is a function, it records the arguments    
    list_t args;
  };
  // ====
  symbol_single_type_t main_type;
  // if the main type is pointer, it points to a var of minor type
  symbol_single_type_t minor_type;
  // e.g. 2-level pointer int**
  int pointer_level;
  // index of the type this type points to
  // so the type will be point_to + pointer_level, i.e.
  // point_to=int*, pointer_level=2, so the type will be char***.
  // well but actually it will put all pointer levels to pointer_level
  // to avoid any * in point_to.
  int point_to;  
} symbol_type_t;
int symtypcmp(int a, int b);
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
  int value_type;
  // used to store constant type in NODE_CONSTANT
  size_t extra_info;
} astnode_t;
typedef enum {
  SYMBOL_VARIABLE,
  SYMBOL_FUNCTION,
} symbol_kind_t;
typedef struct{
  char* name;
  /// this indicates whether it is a variable or function
  symbol_kind_t type;
  
  // index of type in the type table  
  union{
    int sym_type;
    int return_type;
  };
  int layer;
  unsigned long long value;
  int is_extern;
  // function args. the elements are of type symbol_type_t
  list_t args;
}symbol_t;
astnode_t *do_parse(list_t *tokens);

const char* get_nodetype_str(astnode_type_t type);

void free_node(astnode_t *node);
void free_symbol(symbol_t *sym);
void free_all_nodes();
//
#define NODE_DEF_ID(node) (node->left->left)
#define NODE_DEF_TYPEKW(node) (node->left->right)
#define NODE_DEF_REXPR(node) (node->right)

#define NODE_FUNC_ID(node) (node->left->left)
#define NODE_FUNC_TYPEKW(node) (node->left->right)
#define NODE_FUNC_ARGLIST(node) (node->right->left)
#define NODE_FUNC_BODY(node) (node->right->right)

#define NODE_VARDECL_ID(node) (node->left)
#define NODE_VARDECL_TYPEKW(node) (node->right)

#define NODE_FUNCDECL_ID(node) (node->left->left)
#define NODE_FUNCDECL_TYPEKW(node) (node->left->right)
#define NODE_FUNCDECL_ARGLIST(node) (node->right)

#define NODE_CLASS_ID(node) (node->left)
#define NODE_CLASS_MEMBERS(node) (node->right)

#define NODE_ARGPAIR_ID(node) (node->left)
#define NODE_ARGPAIR_TYPEKW(node) (node->right)

