#include "main.h"
#include "stdio.h"
#include "lexer.h"
#include "parser.h"
#include <stddef.h>
#include <stdio.h>


void print_node(astnode_t* node, int indent){
  printf("%*s%s=%s\n",indent,"-",get_nodetype_str(node->node_type), node->value?node->value:"<null>");
  if(node->left)
    print_node(node->left, indent+2); 
  if(node->right)
    print_node(node->right, indent+2);
}
char* code="fn main(){let i=1;i=i+1;if i<=1 {let a=2;}return 0;}";
int main(int argc, char** argv){
  list_t tokens=do_lex(code);
  for (size_t i=0; i < tokens.len; ++i) {
    token_t* tok=list_get(&tokens, i);
    printf("(%d, %s, line %zu, col %zu)\n",tok->token_type, tok->value,
	   tok->position.line, tok->position.column);
  }
  astnode_t asttree=do_parse(&tokens);
  print_node(&asttree, 0);
  
  return 0;
}
