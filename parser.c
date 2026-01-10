#include "lexer.h"
#include "utils.h"
#include "err.h"
#include "parser.h" 
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
typedef astnode_t *(*statment_parser)(list_t *tokens, size_t *iter);
#define NODETYPE_MAP_MAX 50
static char* nodetype_str_map[NODETYPE_MAP_MAX] = {
  [NODE_NONE]="NODE_NONE", 
  // this is usally used when a node has more than 2 subnodes.
  // we put other subnodes under a new leafholder which is under the real parent.
  [NODE_LEAFHOLDER]="NODE_LEAFHOLDER",
  // values
  [NODE_CONSTANT]="NODE_CONSTANT",
  [NODE_IDENTIFIER]="NODE_IDENTIFIER",
  // statements
  [NODE_IF]="NODE_IF",
  [NODE_ELSEIF]="NODE_ELSEIF",
  [NODE_ELSE]="NODE_ELSE",
  [NODE_WHILE]="NODE_WHILE",
  [NODE_RETURN]="NODE_RETURN",

  [NODE_FUNCTION]="NODE_FUNCTION",
  [NODE_DEFINITION]="NODE_DEFINITION",
  // expressions
  [NODE_COMMALIST]="NODE_COMMALIST",
  //operator
  [NODE_ADD]="NODE_ADD",
  [NODE_SUB]="NODE_SUB",
  [NODE_MUL]="NODE_MUL",
  [NODE_DIV]="NODE_DIV",
  [NODE_MOD]="NODE_MOD",
  [NODE_BITAND]="NODE_BITAND",
  [NODE_BITOR]="NODE_BITOR",
  [NODE_XOR]="NODE_XOR",
  //comparator
  [NODE_EQUAL]="NODE_EQUAL",
  [NODE_GREATER]="NODE_GREATER",
  [NODE_LESS]="NODE_LESS",
  [NODE_GREATER_EQUAL]="NODE_GREATER_EQUAL",
  [NODE_LESS_EQUAL]="NODE_LESS_EQUAL",
  [NODE_NOT_EQUAL]="NODE_NOT_EQUAL",
  // action
  [NODE_ASSIGN]="NODE_ASSIGN",
  //logic
  [NODE_AND]="NODE_AND",
  [NODE_OR]="NODE_OR",
  [NODE_NOT]="NODE_NOT",
  //
  [NODE_FUNCCALL]="NODE_FUNCCALL",
  // others
  [NODE_ARGLIST]="NODE_ARGLIST",
  [NODE_ARGPAIR]="NODE_ARGPAIR",
  [NODE_TYPEKW]="NODE_TYPEKW"
};
char *get_nodetype_str(astnode_type_t type) {
  if (type >= NODETYPE_MAP_MAX)
    return NULL;    
  return nodetype_str_map[type];
}
astnode_t* create_node(astnode_type_t type, astnode_t* left, astnode_t* right, char* value){
  astnode_t* node=malloc(sizeof(astnode_t));
  assert(node);
  node->node_type=type;
  node->left=left;
  node->right = right;
  node->value = value;
  node->syms.len = 0;
  node->syms.capacity = 0;
  node->syms.element_size = 0;
  node->syms.array = 0;
  return node;  
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
astnode_t *parse_expr(list_t* tokens, size_t* iter, int precedence);
typedef astnode_t* (*prefix_handler_t)(token_t *lefttoken, list_t *tokens, size_t* iter) ;
typedef astnode_t *(*infix_handler_t)(astnode_t *left, list_t *tokens, size_t* iter);
astnode_t *prefix_handler_id(token_t *lefttoken, list_t *tokens, size_t *iter) {
  // LOG(VERBOSE, "%s ",lefttoken->value);
  (*iter)++;
  return create_node(NODE_IDENTIFIER, NULL, NULL, lefttoken->value);
}
astnode_t* prefix_handler_const(token_t *lefttoken, list_t *tokens, size_t* iter){
  // LOG(VERBOSE, "%s ",lefttoken->value);
  (*iter)++;  
  return create_node(NODE_CONSTANT, NULL, NULL, lefttoken->value);
}
astnode_t* handler_addsub(astnode_type_t type,astnode_t* lhs, astnode_t* rhs){  
  astnode_t* zero=create_node(NODE_CONSTANT, NULL, NULL, "0");  
  return create_node(type, lhs?lhs:zero, rhs, NULL);
}
astnode_t *prefix_handler_add(token_t *lefttoken, list_t *tokens, size_t* iter) {
  astnode_t *left = create_node(
      lefttoken->token_type == IDENTIFIER ? NODE_IDENTIFIER : NODE_CONSTANT,
      NULL, NULL, lefttoken->value);
  (*iter)++;  
  return handler_addsub(NODE_ADD, NULL, left);
}
astnode_t* prefix_handler_minus(token_t *lefttoken, list_t *tokens, size_t* iter){
  astnode_t *left = create_node(
      lefttoken->token_type == IDENTIFIER ? NODE_IDENTIFIER : NODE_CONSTANT,
      NULL, NULL, lefttoken->value);
  (*iter)++;  
  return handler_addsub(NODE_SUB, NULL, left);
}
astnode_t *prefix_handler_openparen(token_t *lefttoken, list_t *tokens,
                                    size_t *iter) {
  size_t backupiter=*iter+1;
  astnode_t *inner = parse_expr(tokens, &backupiter, 0);
  if (peek_check_token(tokens, backupiter+1, CLOSEPAREN)) {
    backupiter++;
    *iter=backupiter;
    return inner;
  }
  cry_error(SENDER_PARSER, "missing closing parenthesis", lefttoken->position);
  return NULL;
}
int precedences[] = {
  [ADD] = 120,
  [SUB] = 120,
  [MUL] = 130,
  [DIV] = 130,
  [MOD] = 130,
  [EQUAL] = 110,
  [GREATER] = 110,
  [LESS] = 110,
  [GREATER_EQUAL] = 110,
  [LESS_EQUAL] = 110,
  [ASSIGN] = 105, // a=1, b=2=>2
  [COMMA] = 100,
  [OPENPAREN] = 1000,
  [CLOSEPAREN] = 0,
  [SEMICOLON] = 0,
  [OPENBRACE] = 0
};
astnode_t *infix_handler_twoop(tokentype_t optype, astnode_t *left,
                               list_t *tokens, size_t *iter) {
  astnode_type_t mapping[] = {
    [ADD] = NODE_ADD,
    [SUB] = NODE_SUB,
    [MUL] = NODE_MUL,
    [DIV] = NODE_DIV,
    [MOD] = NODE_MOD,
    [ASSIGN] = NODE_ASSIGN,

    [EQUAL] = NODE_EQUAL,
    [GREATER] = NODE_GREATER,
    [LESS] = NODE_LESS,
    [GREATER_EQUAL] = NODE_GREATER_EQUAL,
    [LESS_EQUAL] = NODE_LESS_EQUAL,
    [NOT_EQUAL] = NODE_NOT_EQUAL,

    [AND] = NODE_AND,
    [OR] = NODE_OR,

    [COMMA] = NODE_COMMALIST,
     
    [OPENPAREN]=NODE_FUNCCALL,
  };
  astnode_t *right = parse_expr(tokens, iter, precedences[optype]);
  if(!right){
    cry_error(SENDER_PARSER, "missing right expr",
              get_last_token(tokens, *iter)->position);
    return NULL;
  }
  return create_node(mapping[optype], left, right, NULL);
}
#define INFIXHDLR_TWO(tokentype)                                               \
  astnode_t *infix_handler_##tokentype(astnode_t *left, list_t *tokens,        \
                                       size_t *iter) {                         \
    return infix_handler_twoop(tokentype, left, tokens, iter);                 \
  }                                                                            \

INFIXHDLR_TWO(ADD);
INFIXHDLR_TWO(SUB);
INFIXHDLR_TWO(MUL);
INFIXHDLR_TWO(DIV);
INFIXHDLR_TWO(MOD);
INFIXHDLR_TWO(ASSIGN);
INFIXHDLR_TWO(COMMA);

INFIXHDLR_TWO(EQUAL);
INFIXHDLR_TWO(GREATER);
INFIXHDLR_TWO(LESS);
INFIXHDLR_TWO(GREATER_EQUAL);
INFIXHDLR_TWO(LESS_EQUAL);
INFIXHDLR_TWO(NOT_EQUAL);

INFIXHDLR_TWO(AND);
INFIXHDLR_TWO(OR);
INFIXHDLR_TWO(NOT);
INFIXHDLR_TWO(OPENPAREN);
prefix_handler_t prefix_handlers[50] = {
    [IDENTIFIER] = prefix_handler_id,
    [CONSTANT] = prefix_handler_const,
    // operators
    [ADD] = prefix_handler_add,
    [SUB] = prefix_handler_minus,
    [OPENPAREN] = prefix_handler_openparen,
    0
};
infix_handler_t infix_handlers[50] = {
    [ADD] = infix_handler_ADD,
    [SUB] = infix_handler_SUB,
    [MUL] = infix_handler_MUL,
    [DIV] = infix_handler_DIV,
    [ASSIGN] = infix_handler_ASSIGN,
    [MOD] = infix_handler_MOD,
    // compare
    [EQUAL] = infix_handler_EQUAL,
    [GREATER] = infix_handler_GREATER,
    [LESS] = infix_handler_LESS,
    [GREATER_EQUAL] = infix_handler_GREATER_EQUAL,
    [LESS_EQUAL] = infix_handler_LESS_EQUAL,
    [NOT_EQUAL] = infix_handler_NOT_EQUAL,
    // funccall
    [OPENPAREN] = infix_handler_OPENPAREN,

    [COMMA] = infix_handler_COMMA,
    // logic
    [AND] = infix_handler_AND,
    [OR] = infix_handler_OR,
    [NOT] = infix_handler_NOT,
    // bit opertators
};

/*
  parse an expression from the tokens.
  returns an expr-type node.
 */
astnode_t *parse_expr(list_t* tokens, size_t* iter, int precedence){
  size_t backupiter = *iter;
  token_t* left=list_get(tokens, backupiter);
  if (!left)
    return NULL;
  astnode_t* left_node=NULL;
  if (left->token_type < sizeof(prefix_handlers) / sizeof(prefix_handler_t)
                             &&prefix_handlers[left->token_type])
    left_node = prefix_handlers[left->token_type](left, tokens, &backupiter);
  if (!left_node)
    return NULL;
  LOG(VERBOSE, "%s ",left->value);
  // tokens have been consumed in the prefix handlers
  while (1) {
    token_t *peeked = list_get(tokens, backupiter);
    if (!peeked) {
      LOG(VERBOSE, "peeking tokens[%zu] failed. might be the end of tokens.",backupiter);
      break;
    }
    
    if(peeked->token_type>=sizeof(precedences)/sizeof(int)||!precedences[peeked->token_type]){
      // met a token that cannot find its precedence in the array.
      LOG(VERBOSE, "\nparse_expr precedences[%d] not found. quit.\n",peeked->token_type);
      break;
    } else if (precedences[peeked->token_type] <= precedence) {
      LOG(VERBOSE, "\nparse_expr peeked precedences[%s](%d) lower than current one(%d). quit.\n", peeked->value, precedences[peeked->token_type],precedence);      
      break;
    } else if (peeked->token_type <
                   sizeof(infix_handlers) / sizeof(infix_handler_t) &&
               !infix_handlers[peeked->token_type]) {
      // or it cannot be found in the infixhandlers
      LOG(VERBOSE, "\ninfix_handlers[\"%s\"] not found. quit.\n",peeked->value);    
      break;
    }      
    // now actually consume the token
    backupiter++;
    LOG(VERBOSE, "%s ", peeked->value);
    // make up a bigger node
    left_node = infix_handlers[peeked->token_type](left_node, tokens, &backupiter);
    //LOG(VERBOSE, "parse_expr infix handler ok\n");
  }
  LOG(VERBOSE, "\nparse_expr ok. iter:%zu -> %zu.\n", *iter, backupiter);  
  *iter = backupiter;
  return left_node;
}

/*
  parse an identifier from the tokens.
  returns an value node.
 */
astnode_t *parse_identifier(list_t *tokens, size_t *iter) {
  token_t* tok=list_get(tokens, *iter);
  if(!tok||tok->token_type!=IDENTIFIER){
    return NULL;
  }
  // the char* of the token points to a str that lives longer than the nodes
  // so it's ok to directly copy the pointer.
  astnode_t* id=create_node(NODE_IDENTIFIER, NULL, NULL, tok->value);
  assert(id); 
  return id;
}


/*
  parse an type keyword from the tokens.
  returns an typekw node.
 */
astnode_t *parse_typekw(list_t *tokens, size_t *iter) {
  token_t* tok=list_get(tokens, *iter);
  if(!tok/* todo: check the type  */){
    return NULL;
  }
  astnode_t* id=create_node(NODE_TYPEKW, NULL, NULL, tok->value);
  return id;
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
      got=parse_expr(tokens, &backupiter, 0);
      if(!got){
	token_t* tok=get_last_token(tokens, backupiter);
	LOG(VERBOSE, "parse_by_recipe.TOKE_EXPR stopped\n");
	return NULL;
      }
      LOG(VERBOSE, "parse_by_recipe.TOKE_EXPR ok\n");
      list_append(&collected, got);      
      break;
    }
    case TOKEN_STATEMENTS: {
      // put the statements in a list and put the list in the collected
      astnode_t *stmts=create_node(NODE_LEAFHOLDER, NULL, NULL, NULL);
      astnode_t* holder=stmts;
      while (backupiter<tokens->len) {
	astnode_t* stmt=parse_statement(tokens, &backupiter);
	if(stmt){
	  if(holder->left!=NULL){	
	    // needs to create a leafholder
	    astnode_t* new_leafholder=create_node(NODE_LEAFHOLDER, NULL, NULL, NULL);
	    holder->right=new_leafholder;
	    // move to the leafholder
	    holder=new_leafholder;
	  }
	  holder->left=stmt;
	}else{
	  // cannot get a statment. give up moving on.       
	  LOG(VERBOSE, "parse_by_recipe.TOKE_STATEMENTS stopped\n");
	  break;
	}
	// check if we meet the closing brace
	token_t* last=get_last_token(tokens, backupiter);
	if(last->token_type==CLOSEBRACE){
	  break;
	}
      }
      // it's ok even if it's empty
      LOG(VERBOSE, "parse_by_recipe.TOKE_STATEMENTS ok\n");
      list_append(&collected, stmts);
      break;
    }
    case TOKEN_VALUE: {
      // we want an id or const token.
      token_t *tok=list_get(tokens, backupiter);
      if(tok&&(tok->token_type==IDENTIFIER||tok->token_type==CONSTANT)){
	// what we want
        astnode_t *vnode =
            create_node(tok->token_type == IDENTIFIER ? NODE_IDENTIFIER
                                                      : NODE_CONSTANT
                                                            , NULL, NULL, tok->value);
	backupiter++;
	LOG(VERBOSE, "parse_by_recipe.TOKE_VALUE ok\n");
	list_append(&collected, vnode);
      }else{
	LOG(VERBOSE, "parse_by_recipe.TOKE_VALUE stopped\n");
	//cry_error(SENDER_PARSER, "expected identifier or constant", tok->position);
	return NULL;
      }
      break;
    }
    case TOKEN_ID: {
      // we want a id or const token.
      token_t *tok=list_get(tokens, backupiter);
      if(tok&&(tok->token_type==IDENTIFIER)){
	// what we want
	astnode_t* vnode=create_node(NODE_IDENTIFIER, NULL, NULL, tok->value);       
	backupiter++;
	LOG(VERBOSE, "parse_by_recipe.TOKEN_ID ok\n");
	list_append(&collected, vnode);
      }else{
	LOG(VERBOSE, "parse_by_recipe.TOKEN_ID stopped\n");
	//cry_error(SENDER_PARSER, "expected identifier or constant", tok->position);
	return NULL;
      }
      break;
    }
    case TOKEN_ARGLIST: {
      // name type, name type,...
      astnode_t *arglist = create_node(NODE_ARGLIST, NULL, NULL, NULL);
      astnode_t* holder=arglist;
      while (backupiter<tokens->len) {
        astnode_t *id = parse_identifier(tokens, &backupiter);
        astnode_t *argtype = parse_typekw(tokens, &backupiter);
	astnode_t *pair=create_node(NODE_ARGPAIR, id, argtype, NULL);
	if(id&&argtype){ 
	  if(holder->left!=NULL){	
	    // needs to create a leafholder
	    astnode_t* new_leafholder=create_node(NODE_ARGLIST, NULL, NULL, NULL);
	    holder->right=new_leafholder;
	    // move to the leafholder
	    holder=new_leafholder;
	  }
	  holder->left=pair;
	}else{
	  LOG(VERBOSE, "parse_by_recipe.TOKEN_ARGLIST stopped bc id or argtype incomplete\n");
	  break;
        }
        // check if we still have upcoming args
	if(!peek_check_token(tokens, backupiter, COMMA)){
	  LOG(VERBOSE, "parse_by_recipe.TOKEN_ARGLIST stopped bc no comma\n");
	  break;
        }
	// skip the comma        
	backupiter++;
      }
      // it's ok even if it's empty
      LOG(VERBOSE, "parse_by_recipe.TOKEN_ARGLIST ok\n");
      list_append(&collected, arglist);
      break;
    }
    default:{
      // we want a specific type of token.      
      if (!peek_check_token(tokens, backupiter, recipe[i])) {
	token_t *tok=get_last_token(tokens, backupiter);
	// todo: write a func to convert enum into str
	LOG(VERBOSE, "parse_by_recipe.default wanting %d stopped\n",recipe[i]);
	// cry_errorf(SENDER_PARSER, tok->position, "invalid statment: lacking %d", recipe[i]);
	return NULL;
      }
#ifdef NEED_LOG
      LOG(VERBOSE, "parse_by_recipe.default wanting %d ok:", recipe[i]);
      if(LOG_LEVEL<=VERBOSE){
        token_t *t = list_get(tokens, backupiter);
	LOG(VERBOSE, " %s\n",t->value);
      }
#endif      
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

astnode_t* parse_definition(list_t* tokens, size_t* iter){
  tokentype_t rec[]={LET, TOKEN_ID, ASSIGN, TOKEN_EXPR, SEMICOLON};
  list_t* collected=parse_by_recipe(tokens, iter, rec, 5);
  if (!collected) {
    LOG(VERBOSE, "parse_definition failed.\n");
    return NULL;
  }
  astnode_t* id=list_get(collected, 1);  
  astnode_t* rexpr=list_get(collected, 3);
  astnode_t *defnode = create_node(NODE_DEFINITION, id, rexpr, NULL);
  LOG(VERBOSE, "parsed definition.\n");
  return defnode;
}
astnode_t* parse_if(list_t* tokens, size_t* iter){
  tokentype_t rec[]={IF, TOKEN_EXPR, OPENBRACE, TOKEN_STATEMENTS, CLOSEBRACE};
  list_t* collected=parse_by_recipe(tokens, iter, rec, 5);
  if (!collected) {
    LOG(VERBOSE, "parse_if failed.\n");
    return NULL;
  }
  astnode_t* condition=list_get(collected, 1);
  astnode_t* statements=list_get(collected, 3);
  astnode_t* ifnode=create_node(NODE_IF, condition, statements, NULL);
  LOG(VERBOSE, "parsed if.\n");
  return ifnode;
}
astnode_t* parse_elseif(list_t* tokens, size_t* iter){
  tokentype_t rec[]={ELSE, IF, TOKEN_EXPR, OPENBRACE, TOKEN_STATEMENTS, CLOSEBRACE};
  list_t* collected=parse_by_recipe(tokens, iter, rec, 6);
  if (!collected) {
    LOG(VERBOSE, "parse_elseif failed.\n");
    return NULL;
  }
  astnode_t* condition=list_get(collected, 2);
  astnode_t* statements=list_get(collected, 4);
  astnode_t* elseifnode=create_node(NODE_ELSEIF, condition, statements, NULL);
  LOG(VERBOSE, "parsed elseif.\n");
  return elseifnode;
}
astnode_t* parse_else(list_t* tokens, size_t* iter){
  tokentype_t rec[]={ELSE, OPENBRACE, TOKEN_STATEMENTS, CLOSEBRACE};
  list_t* collected=parse_by_recipe(tokens, iter, rec, 4);
  if (!collected) {
    LOG(VERBOSE, "parse_else failed.\n");
    return NULL;
  }
  astnode_t* stmts=list_get(collected, 2);
  astnode_t* elsenode=create_node(NODE_ELSE, stmts, NULL, NULL);
  LOG(VERBOSE, "parsed else.\n");
  return elsenode;
}
astnode_t* parse_while(list_t* tokens, size_t* iter){
  tokentype_t rec[]={WHILE, TOKEN_EXPR, OPENBRACE, TOKEN_STATEMENTS, CLOSEBRACE};
  list_t* collected=parse_by_recipe(tokens, iter, rec, 5);
  if (!collected) {
    LOG(VERBOSE, "parse_while failed.\n");
    return NULL;
  }
  astnode_t* condition=list_get(collected, 1);
  astnode_t* statements=list_get(collected, 3);
  astnode_t* whilenode=create_node(NODE_WHILE, condition, statements, NULL);
  LOG(VERBOSE, "parsed while.\n");
  return whilenode;
}
astnode_t* parse_return(list_t* tokens, size_t* iter){
  tokentype_t rec[]={RETURN, TOKEN_EXPR, SEMICOLON};
  list_t* collected=parse_by_recipe(tokens, iter, rec, 3);
  if (!collected) {
    LOG(VERBOSE, "parse_return failed.\n");
    return NULL;
  }
  astnode_t* expr=list_get(collected, 1);  
  astnode_t* returnnode=create_node(NODE_RETURN, expr, NULL, NULL);
  LOG(VERBOSE, "parsed return.\n");
  return returnnode;
}
astnode_t* parse_function(list_t* tokens, size_t* iter){
  tokentype_t rec[] = {FN,         TOKEN_ID,  OPENPAREN,        TOKEN_ARGLIST,
                       CLOSEPAREN, OPENBRACE, TOKEN_STATEMENTS, CLOSEBRACE};
  list_t *collected = parse_by_recipe(tokens, iter, rec, 8);  
  if (!collected) {
    LOG(VERBOSE, "parse_function failed.\n");
    return NULL;
  }
  astnode_t* id=list_get(collected, 1);
  astnode_t* args=list_get(collected, 3);
  astnode_t *body = list_get(collected, 6);
  // we need an extra leafholder to hold the rest two nodes together.
  astnode_t *two_holder = create_node(NODE_LEAFHOLDER, args, body, NULL);      
  astnode_t* funcnode=create_node(NODE_FUNCTION, id, two_holder, NULL);
  LOG(VERBOSE, "parsed function declaration.\n");
  return funcnode;
}
astnode_t* parse_singleexpr(list_t* tokens, size_t* iter){
   tokentype_t rec[]={TOKEN_EXPR, SEMICOLON};
  list_t* collected=parse_by_recipe(tokens, iter, rec, 2);
  if (!collected) {
    LOG(VERBOSE, "parse_singleexpr failed.\n");
    return NULL;
  }
  astnode_t *expr = list_get(collected, 0);
  // we use leafholder here. it is ok beacuse the value of the expr will
  // be evaled anyway and the desired operations will still be done (func call, assignments etc.)
  astnode_t* singleexprnode=create_node(NODE_LEAFHOLDER, expr, NULL, NULL);
  LOG(VERBOSE, "parsed singleexpr.\n");
  return singleexprnode;
}
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
	astnode_t* new_leafholder=create_node(NODE_LEAFHOLDER, NULL, NULL, NULL);	
	holder->right=new_leafholder;
	// move to the leafholder
	holder=new_leafholder;
      }
      holder->left=stmt;
    }else{
      cry_error(SENDER_PARSER, "met invalid statement", get_last_token(tokens, iter)->position);
      break;
    }
  }
  return root;
}
