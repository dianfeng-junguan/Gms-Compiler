#include "lexer.h"
#include "utils.h"
#include "err.h"
#include "parser.h" 
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
typedef astnode_t* (*statment_parser)(list_t* tokens, size_t* iter);
astnode_t* parse_statement(list_t* tokens, size_t* iter);
/*
  peek a token from the tokens and check if it meets the type.
 */
bool peek_check_token(list_t* tokens, size_t iter, tokentype_t type){
  token_t* tok=list_get(tokens, iter);
  if(!tok)
    return false;
  return tok->token_type==type;
}
/*
  parse an expression from the tokens.
  returns an expr-type node.
 */
astnode_t *parse_expr(list_t* tokens, size_t* iter){
  
  
}

/*
  parse an identifier from the tokens.
  returns an value node.
 */
astnode_t *parse_identifier(list_t *tokens, size_t *iter) {
  token_t* tok=list_get(tokens, *iter);
  if(!tok){
    return NULL;
  }
  astnode_t* id=malloc(sizeof(astnode_t));
  assert(id);
  // the char* of the token points to a str that lives longer than the nodes
  // so it's ok to directly copy the pointer.
  id->value=tok->value;
  id->node_type=NODE_IDENTIFIER;
  id->left=NULL;
  id->right=NULL;
  return id;

}
// get the token at the iter. if iter exceeds the range, try to get the last token
// before iter.
token_t* get_last_token(list_t* tokens, size_t iter){
  token_t* tok=list_get(tokens, iter);
  size_t backward=0;
  // if we are eof, we move backwards to get the position
  while(!tok&&backward<=iter){
    tok=list_get(tokens, iter-backward++);
  }
  assert(tok);
  return tok;
}
// collect the tokens by the recipe. and if ordered expr, value or stmt, it packs up tokens in
// the range into a node.
list_t* parse_by_recipe(list_t* tokens, size_t* iter, tokentype_t recipe[], size_t recipe_len){
  list_t collected=create_list(5, sizeof(token_t));
  size_t backupiter=*iter;
  for (size_t i=0; i<recipe_len; ++i) {
    astnode_t* got=NULL;
    switch (recipe[i]) {
    case TOKEN_EXPR: {
      got=parse_expr(tokens, &backupiter);
      if(!got){
	token_t* tok=get_last_token(tokens, backupiter);
	cry_errorf(SENDER_PARSER, tok->position, "failed to get an expression.");
	return NULL;
      }
      list_append(&collected, got);      
      break;
    }
    case TOKEN_STATEMENTS: {
      // fixme: gen leafholder
      // put the statements in a list and put the list in the collected
      astnode_t *stmts=malloc(sizeof(astnode_t));
      assert(stmts);
      astnode_t* holder=stmts;
      while (backupiter<tokens->len) {
	astnode_t* stmt=parse_statement(tokens, &backupiter);
	if(stmt){
	  if(holder->left!=NULL){	
	    // needs to create a leafholder
	    astnode_t* new_leafholder=malloc(sizeof(astnode_t));
	    assert(new_leafholder);
	    holder->right=new_leafholder;
	    // move to the leafholder
	    holder=new_leafholder;
	  }
	  holder->left=stmt;
	}
	// check if we meet the closing brace
	token_t* last=get_last_token(tokens, backupiter);
	if(last->token_type==CLOSEBRACE){
	  break;
	}
      }
      // it's ok even if it's empty
      list_append(&collected, stmts);
      break;
    }
    case TOKEN_VALUE: {
      // we want a id or const token.
      token_t *tok=get_last_token(tokens, backupiter);
      if(tok&&(tok->token_type==IDENTIFIER||tok->token_type==CONSTANT)){
	// what we want
	astnode_t* vnode=malloc(sizeof(astnode_t));
	assert(vnode);
	vnode->value=tok->value;
	vnode->left=0;
	vnode->right=0;
	vnode->node_type=tok->token_type==IDENTIFIER?NODE_IDENTIFIER:NODE_CONSTANT;
	backupiter++;
	list_append(&collected, vnode);
      }
      cry_error(SENDER_PARSER, "expected identifier or constant", tok->position);
      return NULL;
      break;
    }
    case TOKEN_ID: {
      // we want a id or const token.
      token_t *tok=get_last_token(tokens, backupiter);
      if(tok&&(tok->token_type==IDENTIFIER)){
	// what we want
	astnode_t* vnode=malloc(sizeof(astnode_t));
	assert(vnode);
	vnode->value=tok->value;
	vnode->left=0;
	vnode->right=0;
	vnode->node_type=tok->token_type==IDENTIFIER?NODE_IDENTIFIER:NODE_CONSTANT;
	backupiter++;
	list_append(&collected, vnode);
      }
      cry_error(SENDER_PARSER, "expected identifier or constant", tok->position);
      return NULL;
      break;
    }
    default:{
      // we want a specific type of token.      
      if (!peek_check_token(tokens, backupiter, recipe[i])) {
	token_t *tok=get_last_token(tokens, backupiter);
	// todo: write a func to convert enum into str
	cry_errorf(SENDER_PARSER, tok->position, "invalid statment: lacking %d", recipe[i]);
	return NULL;
      }
      list_append(&collected, list_get(tokens, backupiter));
      backupiter++;
      break;
    }
    }
  }
  //forward the iterator
  *iter=backupiter;
  list_t* retl=malloc(sizeof(list_t));
  *retl=collected;
  return retl;
}

/*
  parse an constant from the tokens.
  returns an value node.
 */
astnode_t *parse_constant(list_t* tokens, size_t* iter){
  
}
astnode_t* parse_definition(list_t* tokens, size_t* iter){
  tokentype_t rec[]={LET, TOKEN_ID, EQUAL, TOKEN_EXPR, SEMICOLON};
  list_t* collected=parse_by_recipe(tokens, iter, rec, 5);
  if (!collected) {
    return NULL;
  }
  astnode_t* defnode=malloc(sizeof(astnode_t));
  assert(defnode);
  defnode->node_type=NODE_DEFINITION;
  defnode->left=list_get(collected, 1);
  defnode->right=list_get(collected, 3);
  return defnode;
}
astnode_t* parse_if(list_t* tokens, size_t* iter){
  tokentype_t rec[]={IF, TOKEN_EXPR, OPENBRACE, TOKEN_STATEMENTS, CLOSEBRACE};
  list_t* collected=parse_by_recipe(tokens, iter, rec, 5);
  if (!collected) {
    return NULL;
  }
  astnode_t* condition=list_get(collected, 1);
  astnode_t* statements=list_get(collected, 3);
  astnode_t* ifnode=malloc(sizeof(astnode_t));
  assert(ifnode);
  ifnode->node_type=NODE_IF;
  ifnode->left=condition;
  ifnode->right=statements;
  return ifnode;
}
astnode_t* parse_elseif(list_t* tokens, size_t* iter){}
astnode_t* parse_else(list_t* tokens, size_t* iter){}
astnode_t* parse_while(list_t* tokens, size_t* iter){}
astnode_t* parse_return(list_t* tokens, size_t* iter){}
astnode_t* parse_function(list_t* tokens, size_t* iter){}
astnode_t* parse_singleexpr(list_t* tokens, size_t* iter){}
static statment_parser recipe_stmts[]={
  parse_definition,
  parse_if,
  parse_elseif,
  parse_else,
  parse_while,
  parse_return,
  parse_function,
  parse_singleexpr,
}; 

astnode_t* parse_statement(list_t* tokens, size_t* iter){
  for (size_t i=0; i < sizeof(recipe_stmts)/sizeof(statment_parser); ++i) {
    size_t backup_iter=*iter;
    astnode_t* node=recipe_stmts[i](tokens,&backup_iter);
    if(node){
      *iter=backup_iter;
      return node;
    }
  }
  return NULL;
}

astnode_t do_parse(list_t* tokens){
  size_t iter=0;
  astnode_t root={0};
  astnode_t* holder=&root;
  while (iter<tokens->len) {
    astnode_t* stmt=parse_statement(tokens, &iter);
    if(stmt){
      if(holder->left!=NULL){	
	// needs to create a leafholder
	astnode_t* new_leafholder=malloc(sizeof(astnode_t));
	assert(new_leafholder);
	holder->right=new_leafholder;
	// move to the leafholder
	holder=new_leafholder;
      }
      holder->left=stmt;
    }
  }
  return root;
}
