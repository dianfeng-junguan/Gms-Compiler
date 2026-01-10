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
  //whitespace
  WHITESPACE,
  //separator
  COMMA,
  SEMICOLON,
  COLON,
  QUOTE,
  DOUBLE_QUOTE,
  OPENPAREN,
  CLOSEPAREN,
  OPENBRACE,
  CLOSEBRACE,
  //operator
  ADD,
  SUB,
  MUL,
  DIV,
  MOD,
  BITAND,
  BITOR,
  XOR,
  //comparator
  EQUAL,
  GREATER,
  LESS,
  GREATER_EQUAL,
  LESS_EQUAL,
  NOT_EQUAL,
  // action
  ASSIGN,
  //logic
  AND,
  OR,
  NOT,// this is also a bit operator
  //values
  IDENTIFIER,
  CONSTANT,
  // below are types used in parser and are not allow to used in lexer.
  TOKEN_VALUE,
  TOKEN_ID,
  TOKEN_EXPR,
  TOKEN_STATEMENTS,
  TOKEN_ARGLIST,
} tokentype_t;

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
token_t *create_token(tokentype_t token_type, char *value, filepos_t pos);
