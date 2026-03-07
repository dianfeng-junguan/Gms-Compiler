/**
   lexer part.
   26/01/05
   Xexux
**/
#include "utils.h"
#include <stddef.h>
#include <stdbool.h>
typedef enum {
  LET,
  FN,
  IF,
  ELSE,
  WHILE,
  RETURN,
  EXTERN,
  BREAK,
  CLASS,
  INCLUDE,
  // type
  INT,
  STRING,
  VOID,
  TYPE_KEYWORD,
  // whitespace
  WHITESPACE,
  // separator
  COMMA,
  SEMICOLON,
  COLON,
  QUOTE,
  DOUBLE_QUOTE,
  OPENPAREN,
  CLOSEPAREN,
  OPEN_SQUAREBRACKET,
  CLOSE_SQUAREBRACKET,
  OPENBRACE,
  CLOSEBRACE,
  // operator
  ADD,
  SUB,
  MUL,
  DIV,
  MOD,
  BITAND,
  BITOR,
  XOR,
  DOT,
  // comparator
  EQUAL,
  GREATER,
  LESS,
  GREATER_EQUAL,
  LESS_EQUAL,
  NOT_EQUAL,
  // action
  ASSIGN,
  // logic
  AND,
  OR,
  // this is also a bit operator
  NOT,
  // values
  IDENTIFIER,
  CONSTANT_NUMBER,
  CONSTANT_STRING,
  CONSTANT_CHAR,
  // below are types used in parser and are not allow to used in lexer.
  TOKEN_VALUE,
  TOKEN_ID,
  TOKEN_TYPEKW,
  TOKEN_EXPR,
  // condition expression  
  TOKEN_EXPR_COND,
  TOKEN_STATEMENTS,
  TOKEN_ARGLIST,
  TOKEN_CLASS_MEMBERDEF,
} tokentype_t;
#define IS_CONST_TOK(tok) ((tok)->token_type==CONSTANT_NUMBER||(tok)->token_type==CONSTANT_STRING)      
typedef struct {
  tokentype_t token_type;
  char* value;
  filepos_t position;
}token_t;

typedef bool (*begin_func_t)(char);
/*
  c: the char to be checked.
  start: the start pointer of the scanned str.
  offset: the offset of the c from start. using this we can know how many chars we have scanned. 
 */
typedef bool (*allow_func_t)(char c,char* start, size_t offset);
typedef bool (*after_func_t)(char* str, list_t* list, filepos_t pos);

typedef struct {
  begin_func_t begin;
  allow_func_t scan;
  after_func_t after;
}lex_recipe_t;

typedef struct{
  char* str;
  tokentype_t op;
}str_op_pair_t;

typedef struct{
  char* str;
  tokentype_t sep;
} str_sep_pair_t;

list_t do_lex(char *str);

/// create a token on the heap.
/// 
/// reminder: the value str needs to be on the heap.
token_t create_token(tokentype_t token_type, char *value, filepos_t pos);
const char* tokentype_tostr(tokentype_t tt);
void free_token(token_t* tok);
