#include "sematic.h"
#include "parser.h"
#include "lexer.h"
#include "err.h"
#include "utils.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/***********************************************************/
/* the checker should decide whether to check the subnodes */
/***********************************************************/

size_t while_depth = 0;
symbol_type_t function_rettype = {TYPE_VOID, TYPE_VOID,0};
bool in_function = false;
list_t *current_func_arglist = NULL;
bool check_node(astnode_t* node,list_t* symbols, int layer);
bool check_arglist(astnode_t* commalist, list_t* symbols, list_t* arglist, size_t index, filepos_t pos, int layer);

/**
   reminder: name must lives longer than the symbol!!!
 **/
symbol_t create_symbol(char* name, symbol_kind_t type, symbol_type_t value_type, int layer){
  return (symbol_t){
    .name=clone_str(name),
    .type=type,
    .sym_type=value_type,
    .layer=layer,
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
bool sym_redef_checker(astnode_t *node, list_t *symbols, int layer) {
  // NODE_DEFINITION  NODE_DECLARE_FUNC  NODE_DECLARE_VAR  NODE_FUNCTION
  // the nodes above all store the identifier at their left subnode
  char *name = node->left->value;
  if(node->node_type==NODE_FUNCTION||node->node_type==NODE_DECLARE_FUNC){
    name = node->left->left->value;    
  }
  if(is_symtab_dup(&node->syms, name)){
    cry_errorf(SENDER_SEMATIC, node->position, "variable redefined or redeclared:%s",
               node->left->value);
    return false;
  }
  if(node->node_type==NODE_FUNCTION){
    // for NODE_FUNCTION, we need to init the symbol table
    init_list(&node->syms, symbols->capacity, sizeof(symbol_t));
    list_copy(&node->syms, symbols, (copy_callback)copy_symbol);
  }
  // ok. add it to the symtab.
  symbol_t defedsym=create_symbol(name, SYMBOL_VARIABLE, (symbol_type_t){TYPE_INT, TYPE_VOID}, layer);
  defedsym.is_extern=true;
  // for func declaration we need to scan the arglist  
  if(node->node_type==NODE_DECLARE_FUNC||node->node_type==NODE_FUNCTION){
    init_list(&defedsym.args, 6, sizeof(symbol_type_t));
    defedsym.type=SYMBOL_FUNCTION;
    if(node->node_type==NODE_FUNCTION){
      node->value_type = node->left->right->value_type;
    }
    current_func_arglist = &defedsym.args;
    in_function=true;
    function_rettype=node->value_type;
    // use the node's symbol tab to prevent the arg variable from leaking into
    // outer scope
    // check the arglist
    if (!check_node(node->node_type == NODE_DECLARE_FUNC ? node->right
                                                         : node->right->left,
                    &node->syms, layer)) {
      /*func node:
        -holder       -holder
         -id -rettype  -args -body
        func decl node:
         -holder        -args
	  -id -rettype
      */
      return false;
    }
    current_func_arglist = NULL;
    function_rettype=(symbol_type_t){TYPE_VOID,TYPE_VOID};
    in_function=false;
  }
  if(node->node_type==NODE_ARGPAIR){
    symbol_t sym=create_symbol(node->left->value, SYMBOL_VARIABLE, node->right->value_type, layer);
    // add it to the function symbol arglist
    // argtype is small enough so we use list_t element to store it directly (though it's void*)
    if(!current_func_arglist){
      cry_error(SENDER_SEMATIC, "function arglist not in a function",
                node->position);      
      return false;
    }
    append(current_func_arglist, &node->right->value_type);
    // ok. add it to the symtab.
    list_append(symbols, &sym);
  }else{
    list_append(symbols, &defedsym);
  }
  // for function and definition we need to check the body or right expr
  if (node->node_type == NODE_FUNCTION) {
    in_function=true;
    function_rettype=node->value_type;
    bool suc = check_node(node->right->right, symbols, layer);
    in_function = false;
    function_rettype=(symbol_type_t){TYPE_VOID,TYPE_VOID};
    if(!suc)return false;
  }else if(node->node_type==NODE_DEFINITION){
    return check_node(node->right,symbols, layer);
  }
  return true;
}
bool sym_undef_trigger(astnode_t *node) {
  return node->node_type==NODE_IDENTIFIER;
}
bool sym_undef_checker(astnode_t *node, list_t *symbols, int layer) {
  for (size_t i=0; i < symbols->len; ++i) {
    symbol_t* sym=list_get(symbols, i);
    assert(node->value);
    if(strcmp(node->value, sym->name)==0){
      // found
      LOG(VERBOSE, "identifier found defined symbol %s, type %d,%d\n",
          node->value, sym->sym_type.main_type, sym->sym_type.minor_type);
      // update the value type of the node      
      node->value_type=sym->sym_type;
      return true;
    }
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
  case NODE_GREATER_EQUAL:case NODE_LESS_EQUAL:
    return true;
  default:
    return false;
    break;
  }
}
bool incontype_checker(astnode_t *node, list_t *symbols, int layer) {
  // infer the type of the expression
  if (!check_node(node->left, symbols, layer) ||
      !check_node(node->right, symbols, layer)) {
    return false;
  }
  if(symtypcmp(node->left->value_type,node->right->value_type)!=0){
    cry_error(SENDER_SEMATIC, "left expression type and right expression type are not the same", node->position);
    return false;
  }
  // infer
  node->value_type = node->left->value_type;
  return true;
}
bool funccall_arg_trigger(astnode_t *node){
  return node->node_type==NODE_FUNCCALL;
}
bool funccall_arg_checker(astnode_t *node, list_t *symbols, int layer) {
  // check left node first
  if(!check_node(node->left, symbols, layer)){
    return false;
  }
  // check args passed on the right  
  for (size_t i=0; i < symbols->len; ++i) {
    symbol_t *sym=list_get(symbols, i);
    if(strcmp(node->left->value, sym->name)==0){
      // infer the type
      node->value_type=sym->return_type;
      // check the args
      if (!check_arglist(node->right, symbols, &sym->args, 0,
                         node->position, layer)) {
	cry_errorf(SENDER_SEMATIC, node->position, "wrong argument type");
	return false;
      }
    }
  }
  return true;
}
bool return_trigger(astnode_t *node) {
  return node->node_type==NODE_RETURN;
}
bool return_checker(astnode_t *node, list_t *symbols, int layer) {
  // check if in function
  if(!in_function){
    cry_error(SENDER_SEMATIC, "return not in function", node->position);
    return false;
  }  
  // check right expr
  symbol_type_t rett={TYPE_VOID,TYPE_VOID};
  if(node->left){
    if(check_node(node->left, symbols, layer))
      rett = node->left->value_type;
    else
      return false;
  }
  // check whether the return type meets the function type
  if(symtypcmp(function_rettype,rett)<0){
    cry_errorf(SENDER_SEMATIC, node->position, "return type does not meet the function type");
    return false;
  }
  return true;
}
bool refer_trigger(astnode_t *node) {
  return node->node_type==NODE_REFER;
}
bool refer_checker(astnode_t *node, list_t *symbols, int layer) {
  if (!node->right || !check_node(node->right, symbols, layer)) {
    cry_error(SENDER_SEMATIC, "invalid right expression", node->position);    
    return false;
  }
  if(node->right->node_type!=NODE_IDENTIFIER){
    // we only allow referring a variable
    cry_error(SENDER_SEMATIC, "trying to refer a non-variable value", node->position);
    return false;
  }
  // infer the type of the expression
  node->value_type.pointer_level=node->right->value_type.pointer_level+1;
  if(node->value_type.pointer_level==1){
    // start to be a pointer
    node->value_type.minor_type=node->right->value_type.main_type;
    node->value_type.main_type=TYPE_POINTER;
  }
  return true;  
}

bool defer_trigger(astnode_t *node){return node->node_type==NODE_DEFER;}
bool defer_checker(astnode_t *node,list_t* symbols, int layer) {  
  if (!node->right || !check_node(node->right, symbols, layer)) {
    cry_error(SENDER_SEMATIC, "invalid right expression", node->position);    
    return false;
  }
  if(node->right->node_type!=NODE_IDENTIFIER){
    // we only allow referring a variable
    cry_error(SENDER_SEMATIC, "trying to refer a non-variable value", node->position);
    return false;
  }
  if(node->right->value_type.main_type!=TYPE_POINTER){
    cry_error(SENDER_SEMATIC, "trying to defer a non-pointer value or variable",
              node->position);
    return false;
  }
  // infer the type of the expression
  node->value_type.pointer_level=node->value_type.pointer_level-1;
  if(node->value_type.pointer_level==0){
    // not a pointer anymore
    node->value_type.main_type=node->right->value_type.minor_type;
    node->value_type.main_type=TYPE_VOID;
  }
  return true;  
}
bool break_trigger(astnode_t *node){return node->node_type==NODE_RETURN;}
bool break_checker(astnode_t *node, list_t *symbols, int layer) {
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
bool condition_block_checker(astnode_t *node, list_t *symbols, int layer) {
  // the body of the if is a scope
  init_list(&node->syms, 10, sizeof(symbol_t));
  list_copy(&node->syms, symbols, (copy_callback)copy_symbol);
  // check body
  if(node->left)
    check_node(node->left, &node->syms, layer);
  else{
    cry_error(SENDER_SEMATIC, "no statment body", node->position);
    return false;
  }
  return true;  
}

bool otherschk_trigger(astnode_t *node){return true;}
bool otherschk_checker(astnode_t* node ,list_t *symbols, int layer) {
  return (node->left?check_node(node->left, symbols, layer):true)&&
    (node->right?check_node(node->right, symbols, layer):true);
}
static rule_t sematic_rules[] = {
  {.name="symbol redefinition",.trigger=sym_redef_trigger,.checker=sym_redef_checker},
  {.name="undefined symbol",.trigger=sym_undef_trigger,.checker=sym_undef_checker},
  {.name="consistent left and right value type",.trigger=incontype_trigger,.checker=incontype_checker},
  {.name="funccall argcheck",.trigger=funccall_arg_trigger,.checker=funccall_arg_checker},
  {.name="return check",.trigger=return_trigger,.checker=return_checker},
  {.name="refer check",.trigger=refer_trigger,.checker=refer_checker},
  {.name="defer check",.trigger=defer_trigger,.checker=defer_checker},
  {.name="break in while",.trigger=break_trigger,.checker=break_checker},
  {.name="scope symbol copier",.trigger=condition_block_trigger,.checker=condition_block_checker},
  {.name="others",.trigger=otherschk_trigger,.checker=otherschk_checker},
};
bool check_node(astnode_t *node, list_t *symbols, int layer) {
  for (size_t i=0; i < sizeof(sematic_rules)/sizeof(rule_t); ++i) {
    rule_t* r=&sematic_rules[i];
    if (r->trigger(node)) {
      LOG(VERBOSE, "checking node %s with %s\n", get_nodetype_str(node->node_type), r->name);
      if(!r->checker(node,symbols, layer)){
	return false;
      }
      break;      
    }
  }
  return true;  
}
bool is_symtab_dup(list_t* syms, char* name){
  for (size_t i=0; i < syms->len; ++i) {
    symbol_t* sym=list_get(syms, i);
    if(strcmp(sym->name, name)==0){
      return true;
    }
  }
  return false;
}
/*
  check the type of passed arguments.
 */
bool check_arglist(astnode_t* commalist, list_t* symbols, list_t* arglist, size_t index, filepos_t pos, int layer){
  if(commalist->node_type==NODE_COMMALIST){
    // commalist. check subnodes
    if(commalist->left){
      if(!check_arglist(commalist->left, symbols, arglist, index, pos, layer))return false;
    }
    if(commalist->right){
      if(!check_arglist(commalist->right, symbols, arglist, index+1, pos, layer))return false;
    }
    return true;
  }
  if(arglist->len<=index){
    cry_error(SENDER_SEMATIC, "too many arguments", pos);
    return false;
  }
  // an expr, check its value type
  if(!check_node(commalist, symbols, layer)){
    return false;
  }
  symbol_type_t passedtype=commalist->value_type;
  symbol_type_t argtype=*(symbol_type_t*)list_get(arglist, index);
  if(symtypcmp(passedtype,argtype)){
    cry_errorf(SENDER_SEMATIC, pos, "expected type %d,%d , found %d,%d\n",
	       (commalist->value_type.main_type),(commalist->value_type.minor_type),
	       (argtype.main_type),(argtype.minor_type));
    return false;
  }
  return true;
  
}
bool do_sematic(astnode_t* ast){
  // init the symbol table
  init_list(&ast->syms, 10, sizeof(symbol_t));
  return check_node(ast, &ast->syms,0);
}
