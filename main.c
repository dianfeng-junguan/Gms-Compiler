#include "main.h"
#include "stdio.h"
#include "lexer.h"
#include <stddef.h>
#include <stdio.h>

char* code="fn main(arg){let i=1;i=i+1;if(i<=1){let a=2;}return 0;}";
int main(int argc, char** argv){
  list_t tokens=do_lex(code);
  for (size_t i=0; i < tokens.len; ++i) {
    token_t* tok=list_get(&tokens, i);
    printf("(%d, %s, line %zu, col %zu)\n",tok->token_type, tok->value,
	   tok->position.line, tok->position.column);
  }
  return 0;
}
