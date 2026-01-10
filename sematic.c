#include "sematic.h"
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
symbol_t* create_symbol(char* name, symbol_type_t type){
  symbol_t* sym=malloc(sizeof(symbol_t));
  assert(sym);
  sym->name=name;
  sym->type=type;
  return sym;
}

bool check_statement(astnode_t* node, list_t* symbols){
  bool success=true;
  filepos_t pos=node->position;
  switch (node->node_type) {
  case NODE_DEFINITION: {
    // check redefinition
    assert(node->left);
    assert(node->right);
    if(node->left->node_type!=NODE_IDENTIFIER){
      cry_error(SENDER_SEMATIC, "expected identifier at the left hand side of =", pos);
    }
    if(is_symtab_dup(&node->syms, node->left->value)){
      cry_error(SENDER_SEMATIC, "variable redefinition", pos);
    }
    // ok. add it to the symtab.
    list_append(symbols, create_symbol(node->left->value, SYMBOL_VARIABLE));
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
    // ok. add it to the symtab.
    list_append(symbols, create_symbol(node->left->value, SYMBOL_FUNCTION));
    // check the inside
    // init the function scope symtab first: inherit the parent symtab.
    init_list(&node->syms, symbols->capacity, symbols->element_size);
    list_copy(&node->syms, symbols);
    if(node->right){
      check_statement(node->right, &node->syms);
    }     
    break;
  }
  case NODE_LEAFHOLDER:
  case NODE_ARGLIST:{
    // directly check the subnodes.
    if(node->left){
      check_statement(node->left, symbols);
    }
    if(node->right){
      check_statement(node->right, symbols);
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
    list_append(symbols, create_symbol(node->left->value, SYMBOL_FUNCTION));
    break;
  }
    
  case NODE_ELSEIF:
    // todo: check if it follows ifnode    
  case NODE_IF: 
    // check condition expr
    if(!node->left){
      cry_error(SENDER_SEMATIC, "no condition expression", pos);
    }
    // check body
    if(node->right)
      check_statement(node->right, symbols);
    else{
      cry_error(SENDER_SEMATIC, "no if body", pos);
    }
    break;
  case NODE_ELSE: {
    // check if follows ifnode or ifelsenode
    // check body
    if(node->left)
      check_statement(node->left, symbols);
    else{
      cry_error(SENDER_SEMATIC, "no if body", pos);
    }
    break;
  }
  case NODE_WHILE: {
    
    break;
  }
  case NODE_RETURN: {
    
    break;
  }
    // two-operand operators
  case NODE_ADD:case NODE_SUB:case NODE_MUL:case NODE_DIV:case NODE_MOD:
  case NODE_BITAND:case NODE_BITOR:case NODE_AND:case NODE_OR:case NODE_COMMALIST:
  case NODE_EQUAL:case NODE_GREATER:case NODE_LESS:case NODE_GREATER_EQUAL:case NODE_LESS_EQUAL:
  {
    // check left and right
    // TODO: check the result of check_expr
    if(node->left){
      check_expr(node->left, symbols);
    }else{
      cry_error(SENDER_SEMATIC, "no if body", pos);
    }
    if(node->right){
      check_expr(node->right, symbols);
    }else{
      cry_error(SENDER_SEMATIC, "no if body", pos);
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
    success=check_statement(ast->left, &ast->syms);
  if(ast->right)
    success=check_statement(ast->right, &ast->syms);
}
