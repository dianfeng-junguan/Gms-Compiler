#include "sematic.h"
#include "parser.h"
#include "err.h"
#include "utils.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
void copy_symbol(symbol_t* old, symbol_t* newt){
  newt->name=clone_str(old->name);
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
size_t while_depth = 0;
symbol_type_t function_rettype = TYPE_VOID;
bool in_function = false;
list_t *current_func_arglist = NULL;
#include "lexer.h"
#define CHK(expr, errmsg, pos)				\
    if(!(expr)){					\
      cry_error(SENDER_SEMATIC, errmsg, pos);		\
      success=false;					\
      break;						\
    }
#define CHKSTMT(node, symbols, layer)                                          \
  if (!check_statement(node, symbols, layer)) {                                \
    success = false;                                                           \
    break;                                                                     \
  }

bool check_statement(astnode_t *node, list_t *symbols, int layer);
/*
  check the type of passed arguments.
 */
bool check_arglist(astnode_t* commalist, list_t* symbols, int layer, list_t* arglist, size_t index, filepos_t pos){
  if(commalist->node_type==NODE_COMMALIST){
    // commalist. check subnodes
    if(commalist->left){
      if(!check_arglist(commalist->left, symbols, layer, arglist, index, pos))return false;
    }
    if(commalist->right){
      if(!check_arglist(commalist->right, symbols, layer, arglist, index+1, pos))return false;
    }
    return true;
  }
  if(arglist->len<=index){
    cry_error(SENDER_SEMATIC, "too many arguments", pos);
    return false;
  }
  // an expr, check its value type
  if(!check_statement(commalist, symbols, layer)){
    return false;
  }
  symbol_type_t passedtype=commalist->value_type;
  symbol_type_t argtype=*(symbol_type_t*)list_get(arglist, index);
  if(passedtype!=argtype){
    cry_errorf(SENDER_SEMATIC, pos, "expected type %d, found %d\n", (commalist->value_type),(argtype));
    return false;
  }
  return true;
  
}
bool check_statement(astnode_t* node, list_t* symbols, int layer){
  bool success=true;
  filepos_t pos=node->position;
  node->layer=layer;
  switch (node->node_type) {    
  case NODE_CONSTANT:{
    // 
    break;
  } 
  case NODE_IDENTIFIER: {
    // if undefined
    bool f=false;
    for (size_t i=0; i < symbols->len; ++i) {
      symbol_t* sym=list_get(symbols, i);
      assert(node->value);
      if(strcmp(node->value, sym->name)==0){
	// found
	LOG(VERBOSE, "identifier found defined symbol %s, type %d\n", node->value, sym->sym_type);
	node->value_type=sym->sym_type;
	f=true;
      }
    }
    if(!f){
      // undefined
      cry_errorf(SENDER_SEMATIC, pos, "undefined variable:%s\n", node->value);
      success=false;
    }        
    break;
  }
    
  case NODE_DECLARE_VAR: 
  case NODE_DECLARE_FUNC: {
    // check redefinition
    assert(node->left);    
    if(node->left->node_type!=NODE_IDENTIFIER){
      cry_error(SENDER_SEMATIC, "expected identifier", pos);
      success=false;
      break;
    }
    if(is_symtab_dup(&node->syms, node->left->value)){
      cry_error(SENDER_SEMATIC, "variable redeclaration", pos);
      success=false;
      break;
    }
    // ok. add it to the symtab.
    symbol_t extsym=create_symbol(node->left->value, SYMBOL_VARIABLE, TYPE_INT, layer);
    extsym.is_extern=true;
    // for func declaration we need to scan the arglist
    init_list(&node->syms, 10, sizeof(symbol_t));
    list_copy(&node->syms, symbols, (copy_callback)copy_symbol);
    if(node->node_type==NODE_DECLARE_FUNC){
      init_list(&extsym.args, 6, sizeof(symbol_type_t));
      current_func_arglist=&extsym.args;
      // use the node's symbol tab to prevent the arg variable
      // leaked into outer scope
      if(!check_statement(node->right, &node->syms, layer)){
	success=false;
      }
      current_func_arglist=NULL;
      if(!success)break;
    }
    list_append(symbols, &extsym);
    break;
  }
  case NODE_DEFINITION: {
    // check redefinition
    assert(node->left);
    assert(node->right);
    if(node->left->node_type!=NODE_IDENTIFIER){
      cry_error(SENDER_SEMATIC, "expected identifier at the left hand side of =", pos);
      success=false;
      break;
    }
    if(is_symtab_dup(&node->syms, node->left->value)){
      cry_error(SENDER_SEMATIC, "variable redefinition", pos);
      success=false;
      break;
    }
    // check rhs
    if(!check_statement(node->right, symbols, layer)){
      success=false;
      break;
    }
    // ok. add it to the symtab.
    // if there is no type explicitly written, we infer the type
    LOG(VERBOSE, "defining %s of type %d\n",node->left->value, node->right->value_type);
    symbol_t sym=create_symbol(node->left->value, SYMBOL_VARIABLE, node->right->value_type, layer);
    list_append(symbols, &sym);
    break;
  }
  case NODE_FUNCTION: {
    // check redefinition
    assert(node->left);
    assert(node->right);
    if(node->left->node_type!=NODE_IDENTIFIER){
      cry_error(SENDER_SEMATIC, "expected identifier at the name of function", pos);
    }
    if(is_symtab_dup(symbols, node->left->value)){
      cry_error(SENDER_SEMATIC, "function name redefinition", pos);
    }
    
    symbol_t func_sym=create_symbol(node->left->value, SYMBOL_FUNCTION, node->value_type, layer);
    // init the function scope symtab first: inherit the parent symtab.
    init_list(&node->syms, symbols->capacity, symbols->element_size);
    list_copy(&node->syms, symbols, (copy_callback)copy_symbol);
    // set the arglist
    init_list(&func_sym.args, 6, sizeof(symbol_type_t));
    current_func_arglist=&func_sym.args;
    // set the vars for return checking
    in_function=true;
    function_rettype=node->value_type;
    if(node->right){
      check_statement(node->right, &node->syms, layer+1);
    }
    function_rettype=TYPE_VOID;
    in_function=false;
    
    current_func_arglist=NULL;
    // ok. add the function to the symtab.
    list_append(symbols, &func_sym);
    break;
  }
  case NODE_LEAFHOLDER:
  case NODE_ARGLIST:{
    // directly check the subnodes.
    if(node->left){
      check_statement(node->left, symbols,layer);
    }
    if(node->right){
      check_statement(node->right, symbols,layer);
    }
    break;
  }
  case NODE_ARGPAIR: {
    // a pair of arg. left name, right type.
    // check redefinition
    assert(node->left);
    assert(node->right);
    if(node->left->node_type!=NODE_IDENTIFIER){
      cry_error(SENDER_SEMATIC, "expected identifier at the name of function", pos);
      success=false;
      break;
    }
    if(is_symtab_dup(symbols, node->left->value)){
      cry_error(SENDER_SEMATIC, "variable redefinition", pos);
      success=false;
      break;
    }
    symbol_type_t argtype=TYPE_VOID;
    if(strcmp(node->right->value, "int")==0){
      argtype=TYPE_INT;
    }else if(strcmp(node->right->value, "string")==0){
      argtype=TYPE_STRING;
    }
    symbol_t sym=create_symbol(node->left->value, SYMBOL_FUNCTION, argtype, layer);
    // add it to the function symbol arglist
    // argtype is small enough so we use list_t element to store it directly (though it's void*)
    if(!current_func_arglist){
      cry_error(SENDER_SEMATIC, "function arglist not in a function", pos);
      success=false;
      break;
    }
    append(current_func_arglist, &argtype);
    // ok. add it to the symtab.
    list_append(symbols, &sym);
    break;
  }
    
  case NODE_ELSEIF:
  case NODE_IF: 
    // check condition expr
    if(!node->left||!check_statement(node->left, symbols, layer)){
      cry_error(SENDER_SEMATIC, "no condition expression", pos);
    }
    // the body of the if is a scope
    init_list(&node->syms, 10, sizeof(symbol_t));
    list_copy(&node->syms, symbols,(copy_callback)copy_symbol);
    // check body
    if(node->right)
      check_statement(node->right, &node->syms,layer+1);
    else{
      cry_error(SENDER_SEMATIC, "no if body", pos);
    }
    break;
  case NODE_ELSE: {
    // the body of the if is a scope
    init_list(&node->syms, 10, sizeof(symbol_t));
    list_copy(&node->syms, symbols, (copy_callback)copy_symbol);
    // check body
    if(node->left)
      check_statement(node->left, &node->syms,layer+1);
    else{
      cry_error(SENDER_SEMATIC, "no else body", pos);
    }
    break;
  }
  case NODE_WHILE: {
    // check condition
    if(node->left){
      if(!check_statement(node->left, symbols, layer)){
	cry_error(SENDER_SEMATIC, "invalid while: invalid condition", node->left->position);
      }
    }else{
      cry_error(SENDER_SEMATIC, "invalid while: no condition", pos);
    }
    // the body of the if is a scope
    init_list(&node->syms, 10, sizeof(symbol_t));
    list_copy(&node->syms, symbols, (copy_callback)copy_symbol);
    // check body
    while_depth++;
    if(node->right)
      check_statement(node->right, &node->syms,layer+1);
    while_depth--;
    break;
  }
  case NODE_RETURN: {
    // check if in function
    if(!in_function){
      cry_error(SENDER_SEMATIC, "return not in function", pos);
      success=false;
      break;
    }
    // check right expr
    symbol_type_t rett=TYPE_VOID;
    if(node->left){
      check_statement(node->left, symbols,layer);
      rett=node->left->value_type;
      success=false;
      break;
    }
    // check whether the return type meets the function type
    if(function_rettype!=rett){
      cry_errorf(SENDER_SEMATIC, pos, "return type does not meet the function type");
      success=false;
      break;
    }
    // its ok if there is no return expr
    break;
  }
      
      
  case NODE_FUNCCALL:{
    // need to check arglist type 
    CHKSTMT(node->left, symbols, layer);
    char* symname=node->left->value;
    for (size_t i=0; i < symbols->len; ++i) {
      symbol_t *sym=list_get(symbols, i);
      if(strcmp(symname, sym->name)==0){
	// infer the type
	node->value_type=sym->return_type;
	// check the args
	CHK(check_arglist(node->right, symbols, layer, &sym->args, 0, pos), "wrong argument", pos);
      }
    }
    CHKSTMT(node->right, symbols, layer);
    break;
  }
    // two-operand operators
  case NODE_ADD:case NODE_SUB:case NODE_MUL:case NODE_DIV:case NODE_MOD:
  case NODE_BITAND:case NODE_BITOR:case NODE_AND:case NODE_OR:case NODE_COMMALIST:
  case NODE_EQUAL:case NODE_GREATER:case NODE_LESS:case NODE_GREATER_EQUAL:case NODE_LESS_EQUAL:  
  case NODE_ASSIGN:
  {
    // check left and right
    // TODO: check the result of check_statement
    if(node->left){
      CHKSTMT(node->left, symbols, layer);
    }else{
      cry_error(SENDER_SEMATIC, "no left expression", pos);
      success=false;
      break;
    }
    if(node->right){
      CHKSTMT(node->right, symbols,layer);
    }else{
      cry_error(SENDER_SEMATIC, "no right expression", pos);
      success=false;
      break;
    }
    // infer the type of the expression
    if(node->left->value_type!=node->right->value_type){
      cry_error(SENDER_SEMATIC, "left expression type and right expression type are not the same", pos);
      success=false;
      break;
    }
    // infer
    node->value_type=node->left->value_type;
    break;
  }
  case NODE_BREAK: {
    if(while_depth==0){
      cry_error(SENDER_SEMATIC, "found break not in while", pos);
    }
    break;
  }
  case NODE_SINGLEEXPR:{
    if(node->left)
      CHKSTMT(node->left, symbols, layer);
    if(node->right) 
      CHKSTMT(node->right, symbols, layer);
    break;
  }
default:
  cry_errorf(SENDER_SEMATIC, pos, "met unsupported node type %s", get_nodetype_str(node->node_type));
  break;
  }
  return success;
}
void free_symbol(symbol_t* sym){
  FREEIFD(sym->name, myfree);
  sym->name=NULL;
  FREEIFD(sym, myfree);
}
bool do_sematic(astnode_t* ast){
  // init the symbol table
  init_list(&ast->syms, 10, sizeof(symbol_t));
  // check every statement
  bool success=true;
  if(ast->left)
    success=check_statement(ast->left, &ast->syms, 0);
  if(!success)return false;
  if(ast->right)
    success=check_statement(ast->right, &ast->syms, 0);
  return success;
}
