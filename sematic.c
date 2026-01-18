#include "sematic.h"
#include "parser.h"
#include "err.h"
#include "utils.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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
symbol_t* create_symbol(char* name, symbol_type_t type, int layer){
  symbol_t* sym=malloc(sizeof(symbol_t));
  assert(sym);
  sym->name=name;
  sym->type=type;
  sym->layer=layer;
  sym->value=0;  
  sym->is_extern=false;
  return sym;
}
symbol_t* create_extern_symbol(char* name, symbol_type_t type, int layer){
  symbol_t* sym=malloc(sizeof(symbol_t));
  assert(sym);
  sym->name=name;
  sym->type=type;
  sym->layer=layer;
  sym->value=0;
  sym->is_extern=true;
  return sym;
}
size_t while_depth=0;
bool check_statement(astnode_t* node, list_t* symbols, int layer){
  bool success=true;
  filepos_t pos=node->position;
  node->layer=layer;
  switch (node->node_type) {    
  case NODE_CONSTANT:{
    // for now we do not check it.
    
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
      cry_error(SENDER_SEMATIC, "expected identifier at the left hand side of =", pos);
      success=false;
    }
    if(is_symtab_dup(&node->syms, node->left->value)){
      cry_error(SENDER_SEMATIC, "variable redefinition", pos);
      success=false;
    }
    // ok. add it to the symtab.
    list_append(symbols, create_symbol(node->left->value, SYMBOL_VARIABLE, layer));
    break;
  }
  case NODE_EXTERN: {
    // if it contains only declaration
    assert(node->left);
    if(node->left->node_type!=NODE_DECLARE_FUNC&&node->left->node_type!=NODE_DECLARE_VAR){
      cry_error(SENDER_SEMATIC, "expected declaration after extern", pos);
      success=false;      
    }else{
      // check redefinition
      if(is_symtab_dup(&node->syms, node->left->value)){
	cry_error(SENDER_SEMATIC, "variable redefinition", pos);
	success=false;
      }
      // ok. add it to the symtab.
      list_append(symbols, create_extern_symbol(node->left->value,
						(node->left->node_type==NODE_DECLARE_VAR?
						SYMBOL_VARIABLE:SYMBOL_FUNCTION), layer));
    }    
    break;
  }
  case NODE_DEFINITION: {
    // check redefinition
    assert(node->left);
    assert(node->right);
    if(node->left->node_type!=NODE_IDENTIFIER){
      cry_error(SENDER_SEMATIC, "expected identifier at the left hand side of =", pos);
      success=false;
    }
    if(is_symtab_dup(&node->syms, node->left->value)){
      cry_error(SENDER_SEMATIC, "variable redefinition", pos);
      success=false;
    }
    // ok. add it to the symtab.
    list_append(symbols, create_symbol(node->left->value, SYMBOL_VARIABLE, layer));
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
    // ok. add the function to the symtab.
    list_append(symbols, create_symbol(node->left->value, SYMBOL_FUNCTION, layer));
    // check the inside
    // init the function scope symtab first: inherit the parent symtab.
    init_list(&node->syms, symbols->capacity, symbols->element_size);
    list_copy(&node->syms, symbols);
    if(node->right){
      check_statement(node->right, &node->syms, layer+1);
    }     
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
    }
    if(is_symtab_dup(symbols, node->left->value)){
      cry_error(SENDER_SEMATIC, "function name redefinition", pos);
    }    
    // ok. add it to the symtab.
    list_append(symbols, create_symbol(node->left->value, SYMBOL_FUNCTION, layer));
    break;
  }
    
  case NODE_ELSEIF:
    // todo: check if it follows ifnode    
  case NODE_IF: 
    // check condition expr
    if(!node->left||!check_statement(node->left, symbols, layer)){
      cry_error(SENDER_SEMATIC, "no condition expression", pos);
    }
    // the body of the if is a scope
    init_list(&node->syms, 10, sizeof(symbol_t));
    list_copy(&node->syms, symbols);
    // check body
    if(node->right)
      check_statement(node->right, &node->syms,layer+1);
    else{
      cry_error(SENDER_SEMATIC, "no if body", pos);
    }
    break;
  case NODE_ELSE: {
    // todo: check if follows ifnode or ifelsenode
    // the body of the if is a scope
    init_list(&node->syms, 10, sizeof(symbol_t));
    list_copy(&node->syms, symbols);
    // check body
    if(node->left)
      check_statement(node->left, &node->syms,layer+1);
    else{
      cry_error(SENDER_SEMATIC, "no if body", pos);
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
    list_copy(&node->syms, symbols);
    // check body
    while_depth++;
    if(node->right)
      check_statement(node->right, &node->syms,layer+1);
    while_depth--;
    break;
  }
  case NODE_RETURN: {
    // check right expr
    if(node->right){
      check_statement(node->left, symbols,layer);
    }
    // its ok if there is no return expr
    break;
  }
    // two-operand operators
  case NODE_ADD:case NODE_SUB:case NODE_MUL:case NODE_DIV:case NODE_MOD:
  case NODE_BITAND:case NODE_BITOR:case NODE_AND:case NODE_OR:case NODE_COMMALIST:
  case NODE_EQUAL:case NODE_GREATER:case NODE_LESS:case NODE_GREATER_EQUAL:case NODE_LESS_EQUAL:
  case NODE_FUNCCALL:  
  case NODE_ASSIGN:
  {
    // check left and right
    // TODO: check the result of check_statement
    if(node->left){
      check_statement(node->left, symbols,layer);
    }else{
      cry_error(SENDER_SEMATIC, "no if body", pos);
    }
    if(node->right){
      check_statement(node->right, symbols,layer);
    }else{
      cry_error(SENDER_SEMATIC, "no if body", pos);
    }
    break;
  }
  case NODE_BREAK: {
    /* TODO: check if it is in while */
    if(while_depth==0){
      cry_error(SENDER_SEMATIC, "found break not in while", pos);
    }
    break;
  }
default:
  cry_error(SENDER_SEMATIC, "met unsupported node type", pos);
  break;
  }
  return success;
}

bool do_sematic(astnode_t* ast){
  // init the symbol table
  init_list(&ast->syms, 10, sizeof(symbol_t));
  // check every statement
  bool success=true;
  if(ast->left)
    success=check_statement(ast->left, &ast->syms, 0);
  if(ast->right)
    success=check_statement(ast->right, &ast->syms, 0);
  return success;
}
