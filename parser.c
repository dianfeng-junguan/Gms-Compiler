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
static size_t node_create_counter=0;
static size_t node_free_counter=0;
void free_node(astnode_t *node) {
  //printf("freed %s=%s@%d type %d\n",get_nodetype_str(node->node_type), node->value?node->value:"<null>",node->layer, node->value_type);
  FREEIFD(node->value,myfree);
  FREEIFD(node,myfree);      
}

astnode_t *create_node(astnode_type_t type, astnode_t *left, astnode_t *right,
                       char *value, filepos_t position) {
  node_create_counter++;
  astnode_t *node = (astnode_t*)myalloc(sizeof(astnode_t));
  assert(node);
  
  assert(node);
  node->node_type=type;
  node->left=left;
  node->right = right;
  if(value){
      size_t slen = strlen(value);
      node->value = malloc(slen + 1);
      strcpy(node->value, value);
  }else{
    node->value=NULL;
  }

  node->syms = NULL;  
  node->position = position;
  node->value_type = 0;
  node->layer = 0;
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
  char* cloned=clone_str(lefttoken->value);
  return create_node(NODE_IDENTIFIER, NULL, NULL, cloned, lefttoken->position);
}
astnode_t* prefix_handler_const(token_t *lefttoken, list_t *tokens, size_t* iter){
  // LOG(VERBOSE, "%s ",lefttoken->value);
  (*iter)++;
  char* value=lefttoken->value;
  astnode_t *node =
      create_node(NODE_CONSTANT, NULL, NULL, value, lefttoken->position);
  node->extra_info=lefttoken->token_type;
  //node->value_type=node_value_type;
  return node;
}
astnode_t* handler_addsub(astnode_type_t type,astnode_t* lhs, astnode_t* rhs){
  filepos_t pos=lhs?lhs->position:rhs->position;
  astnode_t* zero=create_node(NODE_CONSTANT, NULL, NULL, "0", pos);  
  return create_node(type, lhs?lhs:zero, rhs, NULL, pos);
}
#define PREFIX_SINGOP(funcname_noprefix, nodetype_noprefix)                    \
  astnode_t *prefix_handler_##funcname_noprefix(                               \
      token_t *lefttoken, list_t *tokens, size_t *iter) {                      \
    token_t *righttoken = list_get(tokens, *iter + 1);                          \
    if (!righttoken)                                                           \
      return NULL;                                                             \
    astnode_t *left = create_node(                                             \
        righttoken->token_type == IDENTIFIER ? NODE_IDENTIFIER                 \
                                             : NODE_CONSTANT,                  \
        NULL, NULL, clone_str(righttoken->value), righttoken->position);       \
    (*iter)+=2;                                                                 \
    return handler_addsub(NODE_##nodetype_noprefix, NULL, left);               \
  }
PREFIX_SINGOP(add, ADD);
PREFIX_SINGOP(sub, SUB);
PREFIX_SINGOP(refer, REFER);
PREFIX_SINGOP(defer, DEFER)

astnode_t *prefix_handler_openbrace(token_t *lefttoken, list_t *tokens,
                                    size_t *iter) {
  size_t backupiter = *iter + 1;
  // commalist  
  astnode_t *inner = parse_expr(tokens, &backupiter, 0);
  if (peek_check_token(tokens, backupiter, CLOSEBRACE)) {
    backupiter++;
    *iter=backupiter;
    return create_node(NODE_ARRAYFILL, inner, NULL, NULL, lefttoken->position);
  }
  cry_error(SENDER_PARSER, "missing }", lefttoken->position);
  return NULL;
}
astnode_t *prefix_handler_openparen(token_t *lefttoken, list_t *tokens,
                                    size_t *iter) {
  size_t backupiter=*iter+1;
  astnode_t *inner = parse_expr(tokens, &backupiter, 0);
  if (peek_check_token(tokens, backupiter, CLOSEPAREN)) {
    backupiter++;
    *iter=backupiter;
    return inner;
  }
  cry_error(SENDER_PARSER, "missing closing parenthesis", lefttoken->position);
  return NULL;
}

astnode_t *infix_handler_opensquarebracket(astnode_t *left, list_t *tokens,
                                   size_t *iter) {
  size_t backupiter=*iter;
  astnode_t *inner = parse_expr(tokens, &backupiter, 0);
  if (peek_check_token(tokens, backupiter, CLOSE_SQUAREBRACKET)) {
    backupiter++;
    *iter=backupiter;
    return create_node(NODE_INDEX, left, inner, NULL, left->position);
  }
  cry_error(SENDER_PARSER, "missing ]", left->position);
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
    [BITAND] = 130,
    [BITOR] = 130,
    [ASSIGN] = 105,
    [COLON] = 110, // a=1, b=2=>2
    [DOT] = 150,
    [COMMA] = 100,
    [OPENPAREN] = 1000,
    [CLOSEPAREN] = 0,
    [SEMICOLON] = 0,
    [OPENBRACE] = 90,
    [OPEN_SQUAREBRACKET] = 140,
    [CLOSE_SQUAREBRACKET] = 10,
    };
static astnode_type_t mapping[] = {
    [ADD] = NODE_ADD,
    [SUB] = NODE_SUB,
    [MUL] = NODE_MUL,
    [DIV] = NODE_DIV,
    [MOD] = NODE_MOD,
    [ASSIGN] = NODE_ASSIGN,
    [COLON] = NODE_KEYVALUE_PAIR,
    [DOT] = NODE_PROPERTY,

    [EQUAL] = NODE_EQUAL,
    [GREATER] = NODE_GREATER,
    [LESS] = NODE_LESS,
    [GREATER_EQUAL] = NODE_GREATER_EQUAL,
    [LESS_EQUAL] = NODE_LESS_EQUAL,
    [NOT_EQUAL] = NODE_NOT_EQUAL,

    [AND] = NODE_AND,
    [OR] = NODE_OR,

    [COMMA] = NODE_COMMALIST,

    [OPENPAREN] = NODE_FUNCCALL,
    [OPENBRACE] = NODE_CLASSFILL,
};

astnode_t *infix_handler_twoop(tokentype_t optype, astnode_t *left,
                               list_t *tokens, size_t *iter) {
  
  astnode_t *right = parse_expr(tokens, iter, precedences[optype]);
  if(!right){
    cry_error(SENDER_PARSER, "missing right expr",
              get_last_token(tokens, *iter)->position);
    return NULL;
  }
  return create_node(mapping[optype], left, right, NULL, left->position);
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
INFIXHDLR_TWO(COLON);
INFIXHDLR_TWO(DOT);

INFIXHDLR_TWO(EQUAL);
INFIXHDLR_TWO(GREATER);
INFIXHDLR_TWO(LESS);
INFIXHDLR_TWO(GREATER_EQUAL);
INFIXHDLR_TWO(LESS_EQUAL);
INFIXHDLR_TWO(NOT_EQUAL);

INFIXHDLR_TWO(AND);
INFIXHDLR_TWO(OR);
INFIXHDLR_TWO(NOT);

astnode_t *infix_handler_OPENPAREN(astnode_t *left,
                               list_t *tokens, size_t *iter) {
  // we need to parse arglist here rather than commalist
  astnode_t *right = parse_expr(tokens, iter, 0);  
  if(!right){
    cry_error(SENDER_PARSER, "missing right expr",
              get_last_token(tokens, *iter)->position);
    return NULL;
  }
  // consume the close paren  
  if(!peek_check_token(tokens, *iter, CLOSEPAREN)){
    cry_error(SENDER_PARSER, "missing close paren",
              get_last_token(tokens, *iter)->position);
    return NULL;
  }
  (*iter)++;
  return create_node(mapping[OPENPAREN], left, right, NULL, left->position);
}
astnode_t *infix_handler_OPENBRACE(astnode_t *left, list_t *tokens,
                                   size_t *iter) {
  if(left->node_type!=NODE_IDENTIFIER){
    return NULL;
  }
  // we need to parse arglist here rather than commalist
  astnode_t *right = parse_expr(tokens, iter, 0);  
  if(!right){
    cry_error(SENDER_PARSER, "missing right expr",
              get_last_token(tokens, *iter)->position);
    return NULL;
  }
  // consume the close paren  
  if(!peek_check_token(tokens, *iter, CLOSEBRACE)){
    cry_error(SENDER_PARSER, "missing close paren",
              get_last_token(tokens, *iter)->position);
    return NULL;
  }
  (*iter)++;
  return create_node(mapping[OPENBRACE], left, right, NULL, left->position);
}

prefix_handler_t prefix_handlers[50] = {
    [IDENTIFIER] = prefix_handler_id,
    [CONSTANT_NUMBER] = prefix_handler_const,
    [CONSTANT_STRING] = prefix_handler_const,
    [CONSTANT_CHAR] = prefix_handler_const,
    // operators
    [ADD] = prefix_handler_add,
    [SUB] = prefix_handler_sub,
    [MUL] = prefix_handler_defer,
    [BITAND] = prefix_handler_refer,
    [OPENPAREN] = prefix_handler_openparen,
    [OPENBRACE] = prefix_handler_openbrace,
    
    0
};
infix_handler_t infix_handlers[50] = {
    [ADD] = infix_handler_ADD,
    [SUB] = infix_handler_SUB,
    [MUL] = infix_handler_MUL,
    [DIV] = infix_handler_DIV,
    [ASSIGN] = infix_handler_ASSIGN,
    [MOD] = infix_handler_MOD,
    [COLON] = infix_handler_COLON,
    [DOT] = infix_handler_DOT,
    // compare
    [EQUAL] = infix_handler_EQUAL,
    [GREATER] = infix_handler_GREATER,
    [LESS] = infix_handler_LESS,
    [GREATER_EQUAL] = infix_handler_GREATER_EQUAL,
    [LESS_EQUAL] = infix_handler_LESS_EQUAL,
    [NOT_EQUAL] = infix_handler_NOT_EQUAL,
    // funccall
    [OPENPAREN] = infix_handler_OPENPAREN,
    // object def
    [OPENBRACE] = infix_handler_OPENBRACE,
    // indexing
    [OPEN_SQUAREBRACKET] = infix_handler_opensquarebracket,

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
  astnode_t* id=create_node(NODE_IDENTIFIER, NULL, NULL, tok->value, tok->position);
  assert(id);
  (*iter)++;
  return id;
}


astnode_t* recipe_expr(list_t* tokens, size_t* iter, tokentype_t recipe[], size_t recipe_len){
  return parse_expr(tokens, iter, 0);
}
/*
  this function is used to implement object definition, since openbrace might
  be mistaken as the beginning of object definition while it is the beginning
  of if body.
  i.e. classname{name:value}   v.s.   if 1==a {}
  we rise the precedence at the beginning so the expression parsing in the
  condition expression does not take {.
  the side effect of it is objects cannot be defined directly in if condition.
  a pair of parenthesis is needed if you want to do that.
  i.e. if (classname{var:v})==a {}
*/
astnode_t* recipe_expr_cond(list_t* tokens, size_t* iter, tokentype_t recipe[], size_t recipe_len){
  return parse_expr(tokens, iter, precedences[OPENBRACE]);
}
astnode_t* recipe_value(list_t* tokens, size_t* iter, tokentype_t recipe[], size_t recipe_len){
  // we want an id or const token.
  token_t *tok = list_get(tokens, *iter);
  if(tok&&(tok->token_type==IDENTIFIER||IS_CONST_TOK(tok))){
    // what we want
    symbol_type_t value_type={TYPE_VOID,TYPE_VOID};
    // constant
    switch (tok->token_type) {
    case CONSTANT_NUMBER: {
      value_type.main_type=TYPE_INT;
      break;
    }
    case CONSTANT_STRING: {
      value_type.main_type=TYPE_STRING;
      break;
    }
    default:
      // identifier
      break;
    }
    astnode_t *vnode = create_node(
        tok->token_type == IDENTIFIER ? NODE_IDENTIFIER : NODE_CONSTANT, NULL,
        NULL, tok->value, tok->position);
    // vnode->value_type = value_type;
    vnode->extra_info=tok->token_type;
    (*iter)++;
    return vnode;
  }
  return NULL;
}

/*
  parse an type keyword from the tokens.
  returns an typekw node.
 */
astnode_t *recipe_typekw(list_t *tokens, size_t *iter, tokentype_t recipe[],
                         size_t recipe_len) {
  size_t backupiter=*iter;
  astnode_t *typeexpr_root = NULL;
  astnode_t *array_size_node = NULL;
  bool met_open_sqbrk=false;
  bool met_basictype=false;
  while (true) {
    token_t *tok = list_get(tokens, backupiter);
    
    switch (tok->token_type) {
    case INT: 
    case IDENTIFIER:// might be class 
    case VOID: 
    case STRING: {
      // type keyword
      if (typeexpr_root) {
        // already has leaves. this is incorrect because typekw can only be
        // leafnode.
        do_log(VERBOSE, PARSER_OUTPUT,
                   "invalid type: type keyword should be at the front of the "
                   "type expression, rather than being after * or []\n");
	goto freenode;
      }
      typeexpr_root = create_node(NODE_TYPEKW, NULL, NULL,
                                  clone_str(tok->value), tok->position);
      assert(typeexpr_root);      
      met_basictype=true;
      break;
    }
    case MUL: {
      // pointer
      if (!met_basictype) {
        do_log(VERBOSE, PARSER_OUTPUT,
                  "invalid type: * should be after a basic type keyword\n"
                  );
	goto freenode;
      }
      astnode_t *pointer_node =
          create_node(NODE_POINTEROF, typeexpr_root, NULL, NULL, tok->position);
      typeexpr_root = pointer_node;
      break;
    }
    case OPEN_SQUAREBRACKET: {
      met_open_sqbrk = true;
      backupiter++;
      // scan expr
      array_size_node =
          parse_expr(tokens, &backupiter, precedences[CLOSE_SQUAREBRACKET]);
      if (!peek_check_token(tokens, backupiter, CLOSE_SQUAREBRACKET)) {
	// unmatching        
	goto freenode;
      }
      met_open_sqbrk = false;
      if (!array_size_node) {
        //cry_errorf(SENDER_PARSER, tok->position,
        //           "invalid type: invalid expression in []\n");
	goto freenode;
      }
      typeexpr_root = create_node(NODE_ARRAYOF, typeexpr_root, array_size_node, NULL, tok->position);      
      break;
    }
    default:
      do_log(VERBOSE, PARSER_OUTPUT, "invalid type: invalid token: %s\n", tokentype_tostr(tok->token_type));
      goto done;
    }
    
    backupiter++;
  }
done:
  *iter=backupiter;
  return typeexpr_root;
freenode:
  ;  
  // free
  list_t tovisit = create_list(10, sizeof(astnode_t *));
  size_t i = 0;
  append(&tovisit, &typeexpr_root);
  while (i<tovisit.len) {
    astnode_t *v = *(astnode_t**)list_get(&tovisit, i);
    if(v->left)append(&tovisit, &v->left);
    if(v->right)append(&tovisit, &v->right);
    free_node(v);
    i++;
  }
  free_list(&tovisit);  
  return NULL;

}
astnode_t* recipe_id(list_t* tokens, size_t* iter, tokentype_t recipe[], size_t recipe_len){
  // we want an id token.
  token_t *tok=list_get(tokens, *iter);
  if(tok&&(tok->token_type==IDENTIFIER)){
    // what we want
    astnode_t* vnode=create_node(NODE_IDENTIFIER, NULL, NULL, tok->value, tok->position);
    (*iter)++;
    return vnode;
  }
  return NULL;
}
astnode_t* recipe_stmts(list_t* tokens, size_t* iter, tokentype_t recipe[], size_t recipe_len){
  // put the statements in a tree and put the tree in the collected
  filepos_t emptypos={0,0};
  astnode_t *stmts=create_node(NODE_LEAFHOLDER, NULL, NULL, NULL, emptypos);
  astnode_t* holder=stmts;
  while (*iter<tokens->len) {
    astnode_t* stmt=parse_statement(tokens, iter);
    if(stmt){
      if(holder->left!=NULL){	
	// needs to create a leafholder
	astnode_t* new_leafholder=create_node(NODE_LEAFHOLDER, NULL, NULL, NULL, emptypos);
	holder->right=new_leafholder;
	// move to the leafholder
	holder=new_leafholder;
      }
      holder->left=stmt;
    }else{
      // cannot get a statment. give up moving on.
      break;
    }
    // check if we meet the closing brace
    token_t* last=get_last_token(tokens, *iter);
    if (last->token_type == CLOSEBRACE) {
      break;
    }
  }
  // it's ok even if it's empty
  return stmts;
}
astnode_t* recipe_arglist(list_t* tokens, size_t* iter, tokentype_t recipe[], size_t recipe_len){
  // name type, name type,...
  filepos_t emptypos = {0, 0};
  size_t backupiter=*iter;
  astnode_t *arglist = create_node(NODE_ARGLIST, NULL, NULL, NULL, emptypos);
  astnode_t* holder=arglist;
  while (backupiter<tokens->len) {
    token_t *tok=list_get(tokens, backupiter);
    astnode_t *id = parse_identifier(tokens, &backupiter);
    astnode_t *argtype = recipe_typekw(tokens, &backupiter, recipe, recipe_len);
    if(id&&argtype){ 
      astnode_t *pair=create_node(NODE_ARGPAIR, id, argtype, NULL, tok->position);
      if(holder->left!=NULL){
	// needs to create a leafholder
	filepos_t emptypos={0,0};            
	astnode_t* new_leafholder=create_node(NODE_ARGLIST, NULL, NULL, NULL, emptypos);
	holder->right=new_leafholder;
	// move to the leafholder
	holder=new_leafholder;
      }
      holder->left = pair;
    } else {
      if(id)free_node(id);
      if(argtype)free_node(argtype);
      LOG(VERBOSE, "parse_by_recipe.TOKEN_ARGLIST stopped bc id or argtype "
	  "incomplete\n");
      break;
    }
    *iter=backupiter;
    // check if we still have upcoming args
    if(!peek_check_token(tokens, backupiter, COMMA)){
      LOG(VERBOSE, "parse_by_recipe.TOKEN_ARGLIST stopped bc no comma\n");
      LOG(VERBOSE, "*iter=%zu, backupiter=%zu\n", (*iter), backupiter);      
      break;
    } else {
      // skip the comma
      backupiter++; 
    }
  }
  // it's ok even if it's empty
  return arglist;
}

astnode_t* recipe_class_member(list_t* tokens, size_t* iter, tokentype_t recipe[], size_t recipe_len){
  // name type, name type,...
  size_t backupiter=*iter;
  filepos_t emptypos={0,0};      
  astnode_t *members = create_node(NODE_LEAFHOLDER, NULL, NULL, NULL, emptypos);
  astnode_t* holder=members;
  while (backupiter<tokens->len) {
    //LOG(VERBOSE, "%s iter=%zu\n", __FUNCTION__,*iter);
    token_t *tok=list_get(tokens, backupiter);
    astnode_t *id = parse_identifier(tokens, &backupiter);
    astnode_t *memtype = recipe_typekw(tokens, &backupiter, recipe, recipe_len);
    if(id&&memtype){ 
      astnode_t *pair=create_node(NODE_CLASSMEMBER, id, memtype, NULL, tok->position);
      if(holder->left!=NULL){
	// needs to create a leafholder
	filepos_t emptypos={0,0};            
	astnode_t* new_leafholder=create_node(NODE_LEAFHOLDER, NULL, NULL, NULL, emptypos);
	holder->right=new_leafholder;
	// move to the leafholder
	holder=new_leafholder;
      }
      holder->left=pair;
    } else {
      if(id)free_node(id);
      if (memtype)
        free_node(memtype); 
      LOG(VERBOSE, "parse_by_recipe.TOKEN_CLASS_MEMBER_DEF stopped\n");
      LOG(VERBOSE, "iter=%zu,*iter=%s\n",*iter,tokentype_tostr(tok->token_type));      
      return members;
    }
    // skip the semicolon
    backupiter++;
    *iter=backupiter;    
  }
  *iter=backupiter;
  // it's ok even if it's empty
  return members;
}

typedef struct{
  tokentype_t order;
  astnode_t* (*chef)(list_t* tokens, size_t* iter, tokentype_t recipe[], size_t recipe_len);
} recipe_t;
static recipe_t recipes[] = {
    {.order = TOKEN_EXPR, .chef = recipe_expr},
    {.order = TOKEN_EXPR_COND, .chef = recipe_expr_cond},
    {.order = TOKEN_VALUE, .chef = recipe_value},
    {.order = TOKEN_ID, .chef = recipe_id},
    {.order = TOKEN_STATEMENTS, .chef = recipe_stmts},
    {.order = TOKEN_ARGLIST, .chef = recipe_arglist},
    {.order = TOKEN_CLASS_MEMBERDEF, .chef = recipe_class_member},
    {.order=TOKEN_TYPEKW, .chef=recipe_typekw},    
};
// collect the tokens by the recipe. and if ordered expr, value or stmt, it
// packs up tokens in the range into a node. 
list_t* parse_by_recipe(list_t* tokens, size_t* iter, tokentype_t recipe[], size_t recipe_len){
  list_t collected=create_list(5, sizeof(astnode_t*));
  size_t backupiter=*iter;
  for (size_t i=0; i<recipe_len; ++i) {
    astnode_t *got = NULL;
    bool processed=false;
    for (size_t j=0; j < sizeof(recipes)/sizeof(recipe_t); ++j) {
      if(recipes[j].order==recipe[i]){
        processed = true;
        got = recipes[j].chef(tokens, &backupiter, recipe, recipe_len);

	token_t* tok=list_get(tokens, backupiter);        
	LOG(VERBOSE, "backupiter=%zu,type=%d,%s\n",backupiter,tok->token_type,tokentype_tostr(tok->token_type));
	if(got){
          list_append(&collected, &got);
	  break;
        } else {
	  // invalid token array
          LOG(VERBOSE, "parse_by_recipe getting %s failed.\n",
              tokentype_tostr(recipe[i]));
          free_list(&collected);
	  return NULL;
	}
      }
    }
    if (!processed) {
      // we want a specific type of token.
      if(peek_check_token(tokens, backupiter, recipe[i])){
	backupiter++;   
      }else{
        // invalid token array
        token_t *tokk = list_get(tokens, backupiter);
	LOG(VERBOSE, "parse_by_recipe getting %s failed, met %s.\n",
	    tokentype_tostr(recipe[i]),tokentype_tostr(tokk->token_type));
	free_list(&collected);
	return NULL;
      }
    }
  }
  //forward the iterator
  *iter=backupiter;
  list_t* retl=malloc(sizeof(list_t));
  *retl=collected;
  return retl;
}

astnode_t *parse_definition(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos) {
  astnode_t* id=*(astnode_t**)list_get(collected, 0);
  astnode_t* typekw=*(astnode_t**)list_get(collected, 1);
  astnode_t *rexpr = *(astnode_t **)list_get(collected, 2);
  astnode_t* holder=create_node(NODE_LEAFHOLDER, id, typekw, NULL, id->position);
  id->value_type=typekw->value_type;
  astnode_t *defnode =
      create_node(NODE_DEFINITION, holder, rexpr, NULL, pos);
  return defnode;
}

astnode_t *parse_definition_notype(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos) {
  astnode_t* id=*(astnode_t**)list_get(collected, 0);
  astnode_t *rexpr = *(astnode_t **)list_get(collected, 1);
  astnode_t* holder=create_node(NODE_LEAFHOLDER, id, NULL, NULL, id->position);
  astnode_t *defnode =
      create_node(NODE_DEFINITION, holder, rexpr, NULL, pos);
  return defnode;
}

static tokentype_t elseif_recipe[]={ELSE, IF, TOKEN_EXPR, OPENBRACE, TOKEN_STATEMENTS, CLOSEBRACE};
static tokentype_t else_recipe[]={ELSE, OPENBRACE, TOKEN_STATEMENTS, CLOSEBRACE};
astnode_t* parse_else(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos){
  astnode_t* stmts=*(astnode_t**)list_get(collected, 0);
  astnode_t *elsenode =
      create_node(NODE_ELSE, stmts, NULL, NULL, pos);
  return elsenode;
}

astnode_t* parse_elseif(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos){
  astnode_t* condition=*(astnode_t**)list_get(collected, 0);
  astnode_t *statements = *(astnode_t**)list_get(collected, 1);

  astnode_t *nextelseifnode=NULL,*elsenode=NULL;
  list_t *elseif_collected = parse_by_recipe(tokens, iter, elseif_recipe, 6);
  if(elseif_collected){
    // parse elseif
    nextelseifnode = parse_elseif(elseif_collected, tokens, iter, pos);
  }
  list_t *else_collected = parse_by_recipe(tokens, iter, else_recipe, 4);
  if(else_collected){
    // parse else
    elsenode = parse_else(else_collected, tokens, iter, pos);
  }
  astnode_t *branch = nextelseifnode ? nextelseifnode : (elsenode ? elsenode : NULL);
  // make a leafholder to hold the elsenode and statements
  astnode_t *holder = create_node(NODE_LEAFHOLDER, statements, branch,
                                  NULL, pos);

  astnode_t *elseifnode =
      create_node(NODE_ELSEIF, condition, holder, NULL, pos);
  if(elseif_collected){
    free_list(elseif_collected);
    free(elseif_collected);
  }
  if(else_collected){
    free_list(else_collected);
    free(else_collected);  
  }  
  return nextelseifnode;
}
astnode_t* parse_if(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos){
  astnode_t* condition=*(astnode_t**)list_get(collected, 0);
  astnode_t *statements = *(astnode_t**)list_get(collected, 1);

  astnode_t *elseifnode=NULL,*elsenode=NULL;
  list_t *elseif_collected = parse_by_recipe(tokens, iter, elseif_recipe, 6);
  if(elseif_collected){
    // parse elseif
    elseifnode = parse_elseif(elseif_collected, tokens, iter, pos);
  }
  list_t *else_collected = parse_by_recipe(tokens, iter, else_recipe, 4);
  if(else_collected){
    // parse else
    elsenode = parse_else(else_collected, tokens, iter, pos);
  }
  astnode_t* branch=elseifnode?elseifnode:(elsenode?elsenode:NULL);  
  // make a leafholder to hold the elsenode and statements
  astnode_t *holder = create_node(NODE_LEAFHOLDER, statements, branch,
                                  NULL, pos);
  astnode_t *ifnode = create_node(NODE_IF, condition, holder, NULL, pos);

  if(elseif_collected){
    free_list(elseif_collected);
    free(elseif_collected);
  }
  if(else_collected){
    free_list(else_collected);
    free(else_collected);  
  }  
  return ifnode;
}
astnode_t* parse_while(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos){
  astnode_t* condition=*(astnode_t**)list_get(collected, 0);
  astnode_t* statements=*(astnode_t**)list_get(collected, 1);
  astnode_t *whilenode =
      create_node(NODE_WHILE, condition, statements, NULL, pos);
  return whilenode;
}
astnode_t* parse_return(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos){
  astnode_t* expr=*(astnode_t**)list_get(collected, 0);
  astnode_t *returnnode =
      create_node(NODE_RETURN, expr, NULL, NULL, pos);
  
  return returnnode;
}
astnode_t* parse_class(list_t* collected, list_t* tokens, size_t *iter, filepos_t pos){
  astnode_t* idnode=*(astnode_t**)list_get(collected, 0);
  astnode_t *members = *(astnode_t **)list_get(collected, 1);
  astnode_t *classnode = create_node(NODE_CLASS, idnode, members, NULL, pos);
  return classnode;
}
astnode_t* parse_array_index(list_t* collected, list_t* tokens, size_t *iter, filepos_t pos){
  astnode_t* idnode=*(astnode_t**)list_get(collected, 0);
  astnode_t *index = *(astnode_t **)list_get(collected, 1);
  astnode_t *indexnode = create_node(NODE_INDEX, idnode, index, NULL, pos);
  return indexnode;
}
astnode_t* parse_return_noexpr(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos){
  astnode_t *returnnode =
      create_node(NODE_RETURN, NULL, NULL, NULL, pos);
  return returnnode;
}
astnode_t* parse_function(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos){
  astnode_t* id=*(astnode_t**)list_get(collected, 0);
  astnode_t *args = *(astnode_t**)list_get(collected, 1);
  astnode_t *retkw = *(astnode_t**)list_get(collected, 2);
  astnode_t *body = *(astnode_t**)list_get(collected, 3);
  // we need an extra leafholder to hold the rest two nodes together.
  astnode_t *right_holder = create_node(NODE_LEAFHOLDER, args, body, NULL, pos);
  astnode_t *left_holder = create_node(NODE_LEAFHOLDER, id, retkw, NULL, pos);
  astnode_t *funcnode =
      create_node(NODE_FUNCTION, left_holder, right_holder, NULL, pos);
  funcnode->value_type = retkw->value_type;
  return funcnode;
}
astnode_t* parse_singleexpr(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos){
  astnode_t *expr = *(astnode_t**)list_get(collected, 0);
  astnode_t *singleexprnode =
      create_node(NODE_SINGLEEXPR, expr, NULL, NULL, pos);
  return singleexprnode;
}
astnode_t* parse_ext_vardecl(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos){
  /* pattern:
     extern let id:type;
  */
  astnode_t* idnode=*(astnode_t**)list_get(collected, 0);
  astnode_t* typekwnode=*(astnode_t**)list_get(collected, 1);
  astnode_t* declare_node =
    create_node(NODE_DECLARE_VAR, idnode, typekwnode, NULL, idnode->position);
  return declare_node;
}
astnode_t* parse_ext_funcdecl(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos){
  /* pattern:
     extern fn func(arglist):type;
  */
  astnode_t* idnode=*(astnode_t**)list_get(collected, 0);
  astnode_t* arglistnode=*(astnode_t**)list_get(collected, 1);
  astnode_t *rettypenode = *(astnode_t **)list_get(collected, 2);
  astnode_t* holder=create_node(NODE_LEAFHOLDER, idnode, rettypenode, NULL, idnode->position);
  astnode_t* declare_node = create_node(NODE_DECLARE_FUNC, holder, arglistnode, NULL,
					idnode->position);
  return declare_node;
}
astnode_t *parse_break(list_t *collected, list_t *tokens, size_t *iter, filepos_t pos) {
  astnode_t *breaknode =
      create_node(NODE_BREAK, NULL, NULL, NULL, pos);
  return breaknode;
}
typedef struct {
  char name[32];
  tokentype_t *recipe;
  size_t recipe_len;
  astnode_t* (*builder)(list_t* collected, list_t* tokens, size_t* iter, filepos_t pos);
} syntax_rule_t;
#define ttt(name) static tokentype_t name[]
ttt(pat_vardef) = {LET, TOKEN_ID, COLON, TOKEN_TYPEKW, ASSIGN, TOKEN_EXPR,
                                       SEMICOLON};
ttt(pat_vardef_notype) = {LET, TOKEN_ID, ASSIGN, TOKEN_EXPR,
                                       SEMICOLON};
ttt(pat_funcdef) = {
    FN,    TOKEN_ID,     OPENPAREN, TOKEN_ARGLIST,    CLOSEPAREN,
    COLON, TOKEN_TYPEKW, OPENBRACE, TOKEN_STATEMENTS, CLOSEBRACE};
ttt(pat_ext_vardecl) = {EXTERN, LET, TOKEN_ID, COLON, TOKEN_TYPEKW, SEMICOLON};
ttt(pat_ext_funcdecl) = {
    EXTERN,     FN,    TOKEN_ID,     OPENPAREN, TOKEN_ARGLIST,
    CLOSEPAREN, COLON, TOKEN_TYPEKW, SEMICOLON};
ttt(pat_single_expr) = {TOKEN_EXPR, SEMICOLON};
ttt(pat_if)={IF, TOKEN_EXPR_COND, OPENBRACE, TOKEN_STATEMENTS, CLOSEBRACE};
ttt(pat_while) = {WHILE, TOKEN_EXPR_COND, OPENBRACE, TOKEN_STATEMENTS, CLOSEBRACE};
ttt(pat_break) = {BREAK, SEMICOLON};
ttt(pat_return) = {RETURN, TOKEN_EXPR, SEMICOLON};
ttt(pat_return_noexpr) = {RETURN, SEMICOLON};
ttt(pat_class_def) = {CLASS, TOKEN_ID, OPENBRACE, TOKEN_CLASS_MEMBERDEF,
                      CLOSEBRACE};
ttt(pat_index) = {TOKEN_ID, OPEN_SQUAREBRACKET, TOKEN_EXPR, CLOSE_SQUAREBRACKET};
static syntax_rule_t syntaxes[] = {
    {.name = "vardef", .recipe = pat_vardef,  .recipe_len= 7, .builder=parse_definition},
    {.name = "vardef_notype", .recipe = pat_vardef_notype,  .recipe_len= 5, .builder=parse_definition_notype},
    {.name = "funcdef", .recipe= pat_funcdef, .recipe_len= 10, .builder=parse_function},
    {.name = "ext_vardecl", .recipe= pat_ext_vardecl, .recipe_len= 6, .builder=parse_ext_vardecl},
    {.name = "ext_funcdecl", .recipe= pat_ext_funcdecl, .recipe_len= 9, .builder=parse_ext_funcdecl},
    {.name = "single_expr", .recipe= pat_single_expr, .recipe_len= 2, .builder=parse_singleexpr},
    {.name = "if", .recipe= pat_if, .recipe_len= 5, .builder=parse_if},
    {.name = "while", .recipe= pat_while, .recipe_len= 5, .builder=parse_while},
    {.name = "break", .recipe= pat_break, .recipe_len= 2, .builder=parse_break},
    {.name = "return", .recipe= pat_return, .recipe_len= 3, .builder=parse_return},
    {.name = "return_noexpr", .recipe= pat_return_noexpr, .recipe_len= 2, .builder=parse_return},
    {.name = "class_def", .recipe= pat_class_def, .recipe_len= 5, .builder=parse_class},
    {.name = "array_index", .recipe= pat_index, .recipe_len= 4, .builder=parse_array_index},
};

astnode_t *parse_statement(list_t *tokens, size_t *iter) {
  token_t* tok=list_get(tokens, *iter);
  LOG(VERBOSE, "=====================\nnow parsing line %zu\n",tok->position.line);
  for (size_t i=0; i < sizeof(syntaxes)/sizeof(syntax_rule_t); ++i) {
    size_t backup_iter = *iter;
    list_t *collected = parse_by_recipe(tokens, &backup_iter, syntaxes[i].recipe,
                                        syntaxes[i].recipe_len);
    if(!collected){
      LOG(VERBOSE, "parsing with %s failed.\n", syntaxes[i].name);
      continue;
    }
    astnode_t *node = syntaxes[i].builder(collected, tokens, &backup_iter, tok->position);
    free_list(collected);
    free(collected);
    if(node){
      LOG(VERBOSE, "parsed %s.\n",syntaxes[i].name);
      *iter=backup_iter;
      return node;
    }
  }
  return NULL;
}
astnode_t* do_parse(list_t* tokens){
  size_t iter = 0;
  filepos_t emptypos={0,0};
  astnode_t *root = create_node(NODE_LEAFHOLDER, NULL, NULL, NULL, emptypos);
  astnode_t *holder= root;
  while (iter<tokens->len) {
    astnode_t* stmt=parse_statement(tokens, &iter);
    if(stmt){
      if(holder->left!=NULL){
        // needs to create a leafholder
	astnode_t* new_leafholder=create_node(NODE_LEAFHOLDER, NULL, NULL, NULL, emptypos);	
	holder->right=new_leafholder;
	// move to the leafholder
	holder=new_leafholder;
      }
      holder->left=stmt;
    }else{
      cry_error(SENDER_PARSER, "met invalid statement",
                get_last_token(tokens, iter)->position);
      return NULL;
    }
  }
  return root;
}
int symtypcmp(int a, int b) {
  if(a==b){
    return 0;
  }
  return -1;
}
