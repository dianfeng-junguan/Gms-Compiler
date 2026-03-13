#include "sematic.h"
#include "parser.h"
#include "lexer.h"
#include "err.h"
#include "status.h"
#include "utils.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/***********************************************************/
/* the checker should decide whether to check the subnodes */
/***********************************************************/

typedef struct {
  // name of this type  
  char *name;
  // memsize it takes up. class will add up the sizes of the members but the
  // final size depends on the memory alignment chosen.  
  size_t size;
  // if this is a class, it records the members  
  list_t members;
} class_type_t;
int get_type_from_typetree(astnode_t *typekwnode);
size_t while_depth = 0;
int function_rettype = -1;
bool in_function = false;
list_t *current_func_arglist = NULL;
list_t type_table = {0};
#define INTRINSIC_TYPE_INDEX_VOID 0
#define INTRINSIC_TYPE_INDEX_INT 1
#define INTRINSIC_TYPE_INDEX_STRING 2
#define INTRINSIC_TYPE_INDEX_CHAR 3
static symbol_type_t intrinsic_types[] = {
  [INTRINSIC_TYPE_INDEX_VOID]={.name = "void", .size = 0,.type_tree=0},
  [INTRINSIC_TYPE_INDEX_INT]={.name = "int", .size = 8,.type_tree=0},
  [INTRINSIC_TYPE_INDEX_CHAR]={.name = "char", .size = 1,.type_tree=0},
  [INTRINSIC_TYPE_INDEX_STRING]={.name = "string", .size = 8,.type_tree=0},// string is actually just a pointer
    
};
bool check_node(astnode_t* node, compiler_global_data_t* globals);
bool check_arglist(astnode_t *commalist, list_t *arglist, filepos_t pos);
/// check if the two types can be implicitly converted to each other.
bool type_implicitly_convertable(symbol_type_index_t a, symbol_type_index_t b){
  // first check some intrinsic-type-consisted combinations
#define ORDERLESS_COMBINATION(a, b, va, vb)	\
  ((a == va && b == vb) ||			\
   (b == va && a == vb))

  if (ORDERLESS_COMBINATION(a, b, INTRINSIC_TYPE_INDEX_INT,
                             INTRINSIC_TYPE_INDEX_CHAR)||a==b) {
    // char == int
    return true;
  }
  // then get the type trees
  symbol_type_t *atype = list_get(&type_table, a);
  symbol_type_t *btype = list_get(&type_table, b);
  astnode_t* atree=atype->type_tree;
  astnode_t* btree=btype->type_tree;
  bool a_pointer = atree->node_type == NODE_POINTEROF;
  bool b_pointer = btree->node_type == NODE_POINTEROF;
  bool a_array = atree->node_type == NODE_ARRAYOF;
  bool b_array = btree->node_type == NODE_ARRAYOF;
  if ((a_pointer && b == INTRINSIC_TYPE_INDEX_INT) ||
      (b_pointer && a == INTRINSIC_TYPE_INDEX_INT)) {
    // int == pointer
    return true;
  }
  if ((a_pointer && b == INTRINSIC_TYPE_INDEX_STRING) ||
      (b_pointer && a == INTRINSIC_TYPE_INDEX_STRING)) {
    // string == pointer
    return true;
  }
  if ((a_pointer && b_array) ||
      (b_pointer && a_array)) {
    // array == pointer
    return true;
  }
  if (SEMATIC_CHECK==1&&LOG_LEVEL<=VERBOSE) {
    do_log(VERBOSE, SEMATIC_CHECK, "%s failed: \n a tree:\n", __FUNCTION__);
    print_node(atree, 0);
    do_log(VERBOSE, SEMATIC_CHECK, "b tree:\n");
    print_node(btree,0);
  }
  
  return false;
}
/// compare the two tree by structure and node type.
static bool compare_tree(astnode_t* a, astnode_t *b){
  if ((!a && b) || (a && !b)) {
    // one is NULL
    return false;
  } else if (!a && !b) {
    // it's ok if both NULL    
    return true;
  }

  if (!compare_tree(a->left, b->left) ||
      !compare_tree(a->right, b->right)) {    
    return false;
  }
  if (a->node_type==NODE_TYPEKW&&b->node_type==NODE_TYPEKW) {
    return strcmp(a->value, b->value)==0;
  }
  return a->node_type == b->node_type;
}
/// clone a tree
static astnode_t* clone_tree(astnode_t *tree){
  if (!tree) {
    return NULL;
  }
  astnode_t *newone = malloc(sizeof(astnode_t));
  assert(newone);
  memcpy(newone, tree, sizeof(astnode_t));
  newone->left = clone_tree(tree->left);
  newone->right = clone_tree(tree->right);
  if (tree->value) {
    cstring_t cstr = string_from(tree->value);
    newone->value = cstr.data;
  }
  return newone;
}
static size_t calc_typetree_size(astnode_t *tree) {
  if (tree->node_type==NODE_ARRAYOF) {
    size_t element_size = calc_typetree_size(tree->left);    
    size_t element_number = atoi(tree->right->value);
    return element_size * element_number;    
  }else if (tree->node_type==NODE_POINTEROF) {
    return 8;
  }else if (tree->node_type==NODE_TYPEKW) {
    // intrinsic types
    if (strcmp(tree->value, "int")==0) {
      return 8;
    }else if (strcmp(tree->value, "string")==0) {
      return 8;
    }else if (strcmp(tree->value, "char")==0) {
      return 1;
    } else if (strcmp(tree->value, "void") == 0) {      
      return 1;
    }
  }
  // unknown
  return 8;
}
symbol_t create_symbol(char* name, symbol_kind_t type, int value_type){
  return (symbol_t){
    .name=clone_str(name),
    .type=type,
    .sym_type=value_type,
    .layer=0,
    .value=0,  
    .is_extern=false
  };
}
void copy_symbol(symbol_t* old, symbol_t* newt){
  newt->name=clone_str(old->name);
}
void free_symbol(symbol_t* sym){
  FREEIFD(sym->name, myfree);
  sym->name=NULL;
  FREEIFD(sym, myfree);
}

bool sym_redef_trigger(astnode_t *node) {
  switch (node->node_type) {
  case NODE_DEFINITION: 
  case NODE_DECLARE_FUNC:
  case NODE_DECLARE_VAR:
  case NODE_FUNCTION:
  case NODE_ARGPAIR:
    return true;
  default:
    return false;
  }
}
bool sym_redef_checker(astnode_t *node,  compiler_global_data_t *globals) {
  // NODE_DEFINITION  NODE_DECLARE_FUNC  NODE_DECLARE_VAR  NODE_FUNCTION
  // the nodes above all store the identifier at their left subnode
  char *name = node->left->value;
  symbol_type_index_t vartype=0;
  switch (node->node_type) {
  case NODE_DECLARE_FUNC:
    name = NODE_FUNCDECL_ID(node)->value;
    vartype=get_type_from_typetree(NODE_FUNCDECL_TYPEKW(node));    
    break;    
  case NODE_DECLARE_VAR:
    name = NODE_VARDECL_ID(node)->value;
    vartype = get_type_from_typetree(NODE_VARDECL_TYPEKW(node));    
    break;    
  case NODE_FUNCTION:
    name = NODE_FUNC_ID(node)->value;
    // get the return type
    vartype=get_type_from_typetree(NODE_FUNC_TYPEKW(node));
    break;
  case NODE_DEFINITION:
    // check right node first to infer its value type    
    if(!check_node(node->right, globals)){
      return false;
    }
    name = NODE_DEF_ID(node)->value;
    vartype = NODE_DEF_TYPEKW(node)
                  ? get_type_from_typetree(NODE_DEF_TYPEKW(node))
                  : node->right->value_type;
    // check the types of rexpr and type annotation
    if(!type_implicitly_convertable(node->right->value_type,vartype)){
      cry_errorf(SENDER_SEMATIC, node->position,
                 "type annotation does not conform to the right expression: "
                 "expected %d, met %d\n",
                 vartype, node->right->value_type);      
      return false;
    }
    break;
  case NODE_ARGPAIR:
    name = NODE_ARGPAIR_ID(node)->value;
    vartype = get_type_from_typetree(NODE_ARGPAIR_TYPEKW(node));    
    break;    
  default:panic("redef met uncopable node type")break;
  }  
  if(find_symbol(node->syms, name)){
    cry_errorf(SENDER_SEMATIC, node->position, "variable redefined or redeclared:%s",
               node->left->value);
    return false;
  }
  // ok. add it to the symtab.
  symbol_t defedsym =
      create_symbol(name, SYMBOL_VARIABLE, vartype);
  do_log(VERBOSE,SEMATIC_CHECK, "added symbol %s\n", name);
  if(node->node_type==NODE_DECLARE_FUNC||node->node_type==NODE_DECLARE_VAR){
    defedsym.is_extern=true;
  }
  // for func we need to scan the arglist  
  if(node->node_type==NODE_FUNCTION||node->node_type==NODE_DECLARE_FUNC){
    init_list(&defedsym.args, 6, sizeof(name_type_pair_t));
    defedsym.type = SYMBOL_FUNCTION;
    // set the FUNCTION node var_type as return type
    node->value_type = vartype;

    current_func_arglist = &defedsym.args;
    bool prev_infunc = in_function;
    symbol_type_index_t prev_rettype = function_rettype;
    list_t *prev_arglist=current_func_arglist;
    // prev_infunc is used here to store previous status of whether we are in
    // a function. if we are already, then we do not set it to false even if
    // node is a func decl.    
    in_function=in_function|(node->node_type==NODE_DECLARE_FUNC?false:true);
    function_rettype=node->value_type;
    // use the node's symbol tab to prevent the arg variable from leaking into
    // outer scope
    // check the arglist
    if (!check_node(node->node_type == NODE_DECLARE_FUNC ? node->right
                                                         : node->right->left,
                    globals)) {
      /*func node:
        -holder       -holder
         -id -rettype  -args -body
        func decl node:
         -holder        -args
	  -id -rettype
      */
      return false;
    }
    current_func_arglist = prev_arglist;
    function_rettype=prev_rettype;
    in_function=prev_infunc;
  }
  if (node->node_type == NODE_ARGPAIR) {
    if (!current_func_arglist) {
      cry_error(SENDER_SEMATIC, "function arglist not in a function",
		node->position);
    }
    // add it to the func node arglist
    name_type_pair_t arginfo = {.name = clone_str(name), .type = vartype};
    append(current_func_arglist, &arginfo);    
  }
  do_log(VERBOSE, SEMATIC_CHECK, "redef checker added symbol %s\n", name);
  if (!(node->node_type == NODE_ARGPAIR && !in_function)) {
    // exclude func decl situations. their arglist doesn't need to be added
    // to symtab
    add_symbol(node->syms, defedsym);
  }
  // check function body
  if (node->node_type == NODE_FUNCTION) {
    in_function=true;
    function_rettype=vartype;
    bool suc = check_node(node->right->right, globals);
    in_function = false;
    function_rettype=0;
    if(!suc)return false;
  }
  return true;
}
/// find the pointer type of the given type. if there is no such type in the
/// type table, add one to it.
int find_refer_type_of(symbol_type_index_t ind) {
  symbol_type_t *stype = list_get(&type_table, ind);
  if (!stype)
    return -1;
  // construct type tree
  astnode_t newtt = {.node_type = NODE_POINTEROF, .left = stype->type_tree, 0};
  for (size_t i=0; i<type_table.len; i++) {
    symbol_type_t *st = list_get(&type_table, i);
    if (compare_tree(st->type_tree, &newtt)) {
      // found
      return i;
    }
  }
  // no such type. create one
  symbol_type_t newtype = {
      .type_tree = clone_tree(&newtt), .size = calc_typetree_size(&newtt), 0};
  append(&type_table, &newtype);
  return type_table.len - 1;
}
int find_defer_type_of(symbol_type_index_t ind) {  
  symbol_type_t *stype = list_get(&type_table, ind);
  if (!stype||stype->type_tree->node_type!=NODE_POINTEROF)
    return -1;
  // construct type tree
  // remove the POINTEROF node  
  astnode_t *newtt = stype->type_tree->left;
  for (size_t i=0; i<type_table.len; i++) {
    symbol_type_t *st = list_get(&type_table, i);
    if (compare_tree(st->type_tree, newtt)) {
      // found
      return i;
    }
  }
  // no such type. create one
  symbol_type_t newtype = {
      .type_tree = clone_tree(newtt), .size = calc_typetree_size(newtt), 0};
  append(&type_table, &newtype);
  return type_table.len - 1;
}
bool is_pointer_type(symbol_type_index_t ind) {
  symbol_type_t* iter=list_get(&type_table, ind);
  assert(iter);
  return iter->type_tree->node_type==NODE_POINTEROF;
}
/// get the type index judging from the type expression tree
int get_type_from_typetree(astnode_t *typekwnode) {
  // first check if it is a class
  if (typekwnode->node_type==NODE_TYPEKW) {
    for (size_t i=0; i<type_table.len; i++) {
      symbol_type_t *type = list_get(&type_table, i);
      if (strcmp(type->name, typekwnode->value)==0) {
	return i;
      }
    }
  } 
  // see if it matches the existing items
  for (size_t i=0; i<type_table.len; i++) {
    symbol_type_t *type = list_get(&type_table, i);
    if (compare_tree(type->type_tree, typekwnode)) {
      return i;
    }
  }
  // no existing one. add it to the table.
  symbol_type_t newtype = {.type_tree = clone_tree(typekwnode), .size = 8,0};
  // calc size
  newtype.size = calc_typetree_size(typekwnode);
  append(&type_table, &newtype);
  return type_table.len - 1;  
}
bool symtab_setup(astnode_t *node,symbol_table_t *symbols, 
                          compiler_global_data_t *globals) {
  symbol_table_t *passed_down = symbols;
  switch (node->node_type) {
  case NODE_IF:
  case NODE_ELSEIF:
  case NODE_ELSE:
  case NODE_WHILE:
  case NODE_FUNCTION: {
    symbol_table_t symtab = create_symtab(symbols);
    append(&globals->symbol_tables, &symtab);
    node->syms =
        list_get(&globals->symbol_tables, globals->symbol_tables.len - 1);
    passed_down = node->syms;
    break;
  }
  default:
    node->syms=symbols;
    break;
  }
  if(node->left) { symtab_setup(node->left,passed_down, globals);}
  if(node->right) { symtab_setup(node->right,passed_down, globals);}
  return true;
}

bool kwtype_trigger(astnode_t *node) {
  return node->node_type==NODE_TYPEKW;
}
bool kwtype_checker(astnode_t *node,  compiler_global_data_t *globals) {
    int argtype = get_type_from_typetree(node);
    node->value_type = argtype;
    return true;
}

bool ctypeinf_trigger(astnode_t *node) {
  return node->node_type==NODE_CONSTANT||node->node_type==NODE_ARRAYFILL;
}
size_t count_element(astnode_t* commalist){
  size_t n = 0;
  if (commalist->left&&commalist->left->node_type==NODE_CONSTANT) {
    n++;
  }else if (commalist->left) {
    n+=count_element(commalist->left);
  }
  if (commalist->right&&commalist->right->node_type==NODE_CONSTANT) {
    n++;
  }else if (commalist->right) {
    n+=count_element(commalist->right);
  }
  return n;
}
bool ctypeinf_checker(astnode_t *node, compiler_global_data_t *globals) {
  if (node->node_type==NODE_ARRAYFILL) {
    // get element type
    astnode_t *el = node->left;
    while (el->node_type==NODE_COMMALIST) {
      el = el->right;
    }
    symbol_type_index_t element_type = el->value_type;
    // get size
    size_t n = count_element(node->left);
    // construct array type tree
    symbol_type_t *stype = list_get(&type_table, element_type);
    assert(stype);
    astnode_t *elett = stype->type_tree;
    cstring_t cstr = create_string();
    string_sprintf(&cstr, "%d", n);
    astnode_t elen = {.node_type = NODE_CONSTANT, .value = cstr.data, .value_type=INTRINSIC_TYPE_INDEX_INT};
    astnode_t tt = {.node_type = NODE_ARRAYOF, .left = elett, .right = &elen};
    for (size_t i=0; i<type_table.len; i++) {
      symbol_type_t *st = list_get(&type_table, i);
      if (compare_tree(st->type_tree, &tt)) {
        node->value_type = i;
        return true;        
      }
    }
    symbol_type_t newt = {.type_tree = clone_tree(&tt),
                          .size = calc_typetree_size(&tt),
                          .element_size = stype->size,
                          0};    
    append(&type_table, &newt);
    node->value_type = type_table.len - 1;
    do_log(VERBOSE, SEMATIC_CHECK, "ctypeinf arrayfill eletype=%zu, n=%d\n",element_type,n);
    return true;
  }
  switch (node->extra_info) {
  case CONSTANT_CHAR:
    node->value_type = INTRINSIC_TYPE_INDEX_CHAR;
    break;
  case CONSTANT_NUMBER: {
    node->value_type = INTRINSIC_TYPE_INDEX_INT;
    break;
  }
  case CONSTANT_STRING:
    node->value_type = INTRINSIC_TYPE_INDEX_STRING;
    do_log(VERBOSE, SEMATIC_CHECK,"sematic: constant_string set\n");
    break;      
  default:
    cry_errorf(SENDER_SEMATIC, node->position, "met unsupported constant type extra info:%zu\n",node->extra_info);
    break;
  }
  return true;
}
bool sym_undef_trigger(astnode_t *node) {
  return node->node_type==NODE_IDENTIFIER;
}
bool sym_undef_checker(astnode_t *node,  compiler_global_data_t *globals) {
  if (!node->syms)
    return false;
  symbol_table_t *symtab = node->syms;  
  while (symtab) {
    for (size_t i=0; i < symtab->table.len; ++i) {
      symbol_t* sym=list_get(&symtab->table, i);
      assert(node->value);
      if(strcmp(node->value, sym->name)==0){
	do_log(VERBOSE,SEMATIC_CHECK, "identifier found defined symbol %s, type %d\n",
	    node->value, sym->sym_type);
	node->value_type=sym->sym_type;
	return true;
      }
    }
    symtab=symtab->parent;
  }
  cry_errorf(SENDER_SEMATIC, node->position, "undefined variable:%s\n", node->value);
  return false;
}
bool incontype_trigger(astnode_t *node) {
  switch (node->node_type) {
  case NODE_ADD:case NODE_SUB: 
  case NODE_MUL:case NODE_DIV:
  case NODE_MOD:case NODE_AND:
  case NODE_OR:case NODE_BITAND:
  case NODE_BITOR:case NODE_XOR:
  case NODE_ASSIGN:case NODE_EQUAL:
  case NODE_GREATER:case NODE_LESS:
  case NODE_GREATER_EQUAL:
  case NODE_LESS_EQUAL:
  case NODE_COMMALIST:
    return true;
  default:
    return false;
    break;
  }
}
bool incontype_checker(astnode_t *node,  compiler_global_data_t *globals) {
  if (!check_node(node->left, globals) ||
      !check_node(node->right, globals)) {
    return false;
  }
  // if the two types cannot be implicitly converted to the other one  
  if(!type_implicitly_convertable(node->left->value_type,node->right->value_type)){
    cry_error(SENDER_SEMATIC, "left expression type and right expression type are not the same", node->position);
    return false;
  }
  node->value_type = node->left->value_type;
  return true;
}
bool funccall_arg_trigger(astnode_t *node){
  return node->node_type==NODE_FUNCCALL;
}
bool funccall_arg_checker(astnode_t *node,  compiler_global_data_t *globals) {
  if(!check_node(node->left, globals)){
    return false;
  }
  if (!node->syms)
    return false;
  symbol_t *sym=find_symbol(node->syms, node->left->value);  
  node->value_type=sym->return_type;
  if (!check_arglist(node->right, &sym->args, node->position)) {    
    cry_errorf(SENDER_SEMATIC, node->position, "wrong argument type");
    return false;
  }
  // infer type of this funccall
  symbol_t *func = find_symbol(node->syms, node->left->value);
  assert(func);
  node->value_type = func->return_type;  
  return true;
}
bool exismem_trigger(astnode_t *node) {
  return node->node_type==NODE_PROPERTY;
}
bool exismem_checker(astnode_t *node,  compiler_global_data_t *globals) {
  if (!node->syms) return false;
  symbol_t *sym = find_symbol(node->syms, node->left->value);
  if (!sym) {
    cry_errorf(SENDER_SEMATIC, node->position, "undefined symbol %s\n",node->left->value);
    return false;
  }
  symbol_type_t *stype = list_get(&type_table, sym->sym_type);
  char *memname = node->right->value;
  for (size_t i=0; i < stype->members.len; ++i) {
    name_type_pair_t *p = list_get(&stype->members, i);
    if (strcmp(memname, p->name) == 0) {
      node->value_type=p->type;
      return true;
    }
  }
  cry_errorf(SENDER_SEMATIC, node->position, "member %s not found in %s\n",
             memname, sym->name);
  return false;
}
bool return_trigger(astnode_t *node) {
  return node->node_type==NODE_RETURN;
}
bool return_checker(astnode_t *node,  compiler_global_data_t *globals) {
  if(!in_function){
    cry_error(SENDER_SEMATIC, "return not in function", node->position);
    return false;
  }  
  int rett=-1;
  if(node->left){
    if(check_node(node->left, globals))
      rett = node->left->value_type;
    else
      return false;
  }
  if(!type_implicitly_convertable(function_rettype,rett)){
    cry_errorf(SENDER_SEMATIC, node->position, "return type does not meet the function type");
    return false;
  }
  return true;
}
bool refer_trigger(astnode_t *node) {
  return node->node_type==NODE_REFER;
}
bool refer_checker(astnode_t *node,  compiler_global_data_t *globals) {
  if (!node->right || !check_node(node->right, globals)) {
    cry_error(SENDER_SEMATIC, "invalid right expression", node->position);    
    return false;
  }
  if(node->right->node_type!=NODE_IDENTIFIER){
    cry_error(SENDER_SEMATIC, "trying to refer a non-variable value", node->position);
    return false;
  }
  node->value_type=find_refer_type_of(node->right->value_type);
  return true;  
}

bool defer_trigger(astnode_t *node){return node->node_type==NODE_DEFER;}
bool defer_checker(astnode_t *node, compiler_global_data_t *globals) {  
  if (!node->right || !check_node(node->right, globals)) {
    cry_error(SENDER_SEMATIC, "invalid right expression", node->position);    
    return false;
  }
  if(node->right->node_type!=NODE_IDENTIFIER){
    cry_error(SENDER_SEMATIC, "trying to refer a non-variable value", node->position);
    return false;
  }  
  if(!is_pointer_type(node->right->value_type)){
    cry_error(SENDER_SEMATIC, "trying to defer a non-pointer value or variable",
              node->position);
    return false;
  }
  node->value_type=find_defer_type_of(node->right->value_type);
  return true;  
}
bool break_trigger(astnode_t *node){return node->node_type==NODE_BREAK;}
bool break_checker(astnode_t *node,  compiler_global_data_t *globals) {
  if(while_depth==0){
    cry_error(SENDER_SEMATIC, "found break not in while", node->position);
    return false;
  }
  return true;;  
}

bool condition_block_trigger(astnode_t *node){
  switch (node->node_type) {
  case NODE_IF: 
  case NODE_ELSEIF: 
  case NODE_ELSE: 
  case NODE_WHILE: 
    return true;
  default:
    return false;
  }
}
bool condition_block_checker(astnode_t *node,  compiler_global_data_t *globals) {
  init_list(&node->syms->table, 10, sizeof(symbol_t));
  list_copy(&node->syms->table, &node->syms->table, (copy_callback)copy_symbol);
  if(node->left)
    check_node(node->left, globals);
  else{
    cry_error(SENDER_SEMATIC, "no statment body", node->position);
    return false;
  }
  return true;  
}

bool otherschk_trigger(astnode_t *node) {
  return node->node_type==NODE_LEAFHOLDER||node->node_type==NODE_ARGLIST;
}
bool otherschk_checker(astnode_t* node , compiler_global_data_t *globals) {
  return (node->left?check_node(node->left, globals):true)&&
    (node->right?check_node(node->right, globals):true);
}

bool objdef_typeinf_trigger(astnode_t *node) {
  return node->node_type==NODE_CLASSFILL;
}
bool objdef_typeinf_checker(astnode_t *node,  compiler_global_data_t *globals){
#define NODE_CLASSFILL_CLASSNAME(node) (node->left)
  char *classname = NODE_CLASSFILL_CLASSNAME(node)->value;
  for (size_t i=0; i < type_table.len; ++i) {
    symbol_type_t *stype = list_get(&type_table, i);
    if (stype->name&&strcmp(stype->name, classname) == 0) {
      node->value_type = i;
      return true;     
    }
  }
  cry_errorf(SENDER_SEMATIC, node->position, "class not found:%s\n",classname);
  return false;
}
bool class_def_trigger(astnode_t *node) {
  return node->node_type == NODE_CLASS;
}
void init_sematic(){
  init_list(&type_table, 10, sizeof(symbol_type_t));
  astnode_t *void_tree=create_node(NODE_TYPEKW,NULL,NULL,"void",(filepos_t){0});
  astnode_t *int_tree=create_node(NODE_TYPEKW,NULL,NULL,"int",(filepos_t){0});
  astnode_t *string_tree=create_node(NODE_TYPEKW,NULL,NULL,"string",(filepos_t){0});
  astnode_t *char_tree=create_node(NODE_TYPEKW,NULL,NULL,"char",(filepos_t){0});
  intrinsic_types[INTRINSIC_TYPE_INDEX_VOID].type_tree = void_tree;
  intrinsic_types[INTRINSIC_TYPE_INDEX_INT].type_tree = int_tree;
  intrinsic_types[INTRINSIC_TYPE_INDEX_STRING].type_tree = string_tree;
  intrinsic_types[INTRINSIC_TYPE_INDEX_CHAR].type_tree = char_tree;
  for (int i = 0; i < sizeof(intrinsic_types) / sizeof(symbol_type_t); i++) {
    append(&type_table, &intrinsic_types[i]);
  }
}
bool class_def_checker(astnode_t *node,  compiler_global_data_t *globals) {
  symbol_type_t class_type = {
    .name = clone_str(node->left->value),
    .size=0,
    .members = create_list(10, sizeof(name_type_pair_t))
  };  
  list_t tovisit = create_list(32, sizeof(astnode_t *));
  append(&tovisit, &node->right);
  size_t i = 0;
  size_t class_size = 0;  
  while (i < tovisit.len) {
    astnode_t *subnode = *(astnode_t **)list_get(&tovisit, i);
    if (!subnode) {
      i++;
      continue;
    }
    if (subnode->node_type==NODE_CLASSMEMBER){
      astnode_t *idnode = subnode->left;
      astnode_t *typenode = subnode->right;
      name_type_pair_t member = {.name = clone_str(idnode->value),
                                 .type = get_type_from_typetree(typenode)};
      symbol_type_t *stype = list_get(&type_table, member.type);
      class_size += stype->size;      
      append(&class_type.members, &member);
    }else if(subnode->node_type==NODE_LEAFHOLDER){
      if(subnode->left)append(&tovisit, &subnode->left);
      if(subnode->right)append(&tovisit, &subnode->right);
    }    
    i++;
  }
  class_type.size = class_size;
  class_type.type_tree = clone_tree(node);  
  do_log(VERBOSE, SEMATIC_CHECK, "class %s size=%zu\n",class_type.name,class_size);
  append(&type_table, &class_type);
  do_log(VERBOSE,SEMATIC_CHECK, "class type registered:%s\n",class_type.name);
  free_list(&tovisit);
  return true;
}
static rule_t sematic_preprocess_list[] = {
    {.name = "keyword type infer",
     .trigger = kwtype_trigger,
     .checker = kwtype_checker},
    {.name = "constant type infer",
     .trigger = ctypeinf_trigger,
     .checker = ctypeinf_checker},
    {.name = "class definition",
     .trigger = class_def_trigger,
     .checker = class_def_checker},
    {.name = "object definition type infer",
     .trigger = objdef_typeinf_trigger,
     .checker = objdef_typeinf_checker}    
};

static rule_t sematic_rules[] = {
    {.name = "symbol redefinition",
     .trigger = sym_redef_trigger,
     .checker = sym_redef_checker},
    {.name = "undefined symbol",
     .trigger = sym_undef_trigger,
     .checker = sym_undef_checker},
    {.name = "consistent left and right value type",
     .trigger = incontype_trigger,
     .checker = incontype_checker},
    {.name = "funccall argcheck",
     .trigger = funccall_arg_trigger,
     .checker = funccall_arg_checker},
    {.name = "existent member",
     .trigger = exismem_trigger,
     .checker = exismem_checker},
    {.name = "return check",
     .trigger = return_trigger,
     .checker = return_checker},
    {.name = "refer check", .trigger = refer_trigger, .checker = refer_checker},
    {.name = "defer check", .trigger = defer_trigger, .checker = defer_checker},
    {.name = "break in while",
     .trigger = break_trigger,
     .checker = break_checker},
    {.name = "scope symbol copier",
     .trigger = condition_block_trigger,
     .checker = condition_block_checker},
  {.name="others",.trigger=otherschk_trigger,.checker=otherschk_checker},
};
bool check_node(astnode_t *node, compiler_global_data_t* globals) {
  for (size_t i=0; i < sizeof(sematic_rules)/sizeof(rule_t); ++i) {
    rule_t* r=&sematic_rules[i];
    if (r->trigger(node)) {
      do_log(VERBOSE, SEMATIC_CHECK, "checking node %s with %s\n", get_nodetype_str(node->node_type), r->name);
      if(!r->checker(node, globals)){
	return false;
      }      
    }
  }
  return true;
}
void tree_to_list(astnode_t* node, list_t *list){
  switch (node->node_type) {
  case NODE_COMMALIST: {
    if(node->left)tree_to_list(node,list);
    if(node->right)tree_to_list(node,list);
    break;
  }
  default:
    append(list, node);
    break;
  }
}
bool check_arglist(astnode_t *commalist, list_t *arglist, filepos_t pos) {  
  list_t passed_args = create_list(10, sizeof(astnode_t));
  tree_to_list(commalist, &passed_args);
  bool r=true;
  if(arglist->len!=passed_args.len){
    cry_errorf(SENDER_SEMATIC, pos, "argument numbers does not match: expected %zu arguments, met %zu\n", arglist->len, passed_args.len);
    r = false;
    goto endfree; 
  }
  if(!check_node(commalist, NULL)){
    r = false;
    goto endfree;
  }
  for (size_t i = 0; i < arglist->len; i++) {
    astnode_t* passedtype=list_get(&passed_args, i);
    name_type_pair_t *argtype=list_get(arglist, i);
    if (!type_implicitly_convertable(passedtype->value_type, argtype->type)) {    
      cry_errorf(SENDER_SEMATIC, pos, "wrong argument:expected type %d , found %d\n",
		 (argtype->type),(passedtype->value_type));
      r = false;
      goto endfree;
    }
  }
 endfree:  
  free_list(&passed_args);
  return r;
}
bool preprocess_tree(astnode_t* ast, compiler_global_data_t* globals){
  bool suc = true;
  if(ast->left)suc=preprocess_tree(ast->left, globals)?suc:false;
  if(ast->right)suc=preprocess_tree(ast->right, globals)?suc:false;
  for (int i = 0; i < sizeof(sematic_preprocess_list) / sizeof(rule_t); ++i) {

    if (sematic_preprocess_list[i].trigger(ast)) {
      if(!sematic_preprocess_list[i].checker(ast, globals)){
	suc=false;
      }
    }
  }
  return suc;
}
bool do_sematic(astnode_t* ast, compiler_global_data_t* globals){
  symbol_table_t global_symtab = create_symtab(NULL);
  append(&globals->symbol_tables, &global_symtab);

  ast->syms = list_get(&globals->symbol_tables, 0);
  // setup symtab
  symtab_setup(ast, ast->syms, globals);
  // preprocess
  if(!preprocess_tree(ast, globals)){
    cry_error(SENDER_SEMATIC, "failed preprocessing", ast->position);
  }
  return check_node(ast, globals);
}
size_t get_typesize(symbol_type_index_t typeindex) {
  symbol_type_t *stype = list_get(&type_table, typeindex);
  assert(stype);
  return stype->size;  
}

symbol_table_t create_symtab(symbol_table_t *parent) {
  
  symbol_table_t symtab = {.parent = parent,
                           .table = create_list(10, sizeof(symbol_t))};
  return symtab;
}
void add_symbol(symbol_table_t *symtab, symbol_t sym){
  append(&symtab->table, &sym);
}
symbol_t *find_symbol(symbol_table_t *symtab, char *name){
  for (size_t i=0; i < symtab->table.len; ++i) {
    symbol_t *sym = list_get(&symtab->table, i);
    if (strcmp(sym->name, name)==0) {
      return sym;
    }
  }
  if (symtab->parent) {
    return find_symbol(symtab->parent, name);
  }
  return NULL;
}
void free_symtab(symbol_table_t *symtab) {
  free_list(&symtab->table);
  symtab->parent = NULL;  
}
