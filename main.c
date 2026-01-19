#include "main.h"
#include "stdio.h"
#include "lexer.h"
#include "parser.h"
#include "sematic.h"
#include "intercode.h"
#include "asmgen.h"
#include "utils.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void print_node(astnode_t* node, int indent){
  printf("%*s%s=%s@%d type %d\n",indent,"-",get_nodetype_str(node->node_type), node->value?node->value:"<null>",node->layer, node->value_type);
  if(node->left)
    print_node(node->left, indent+2); 
  if(node->right)
    print_node(node->right, indent+2);
}
int main(int argc, char** argv){
  if(argc<2){
    perror("error: need input file\n");
    return -1;
  }
  init_my_allocator();
  char* input=argv[1];
  char* output=argc<3?"a.asm":argv[2];
  FILE* f=fopen(input, "r");
  if(!f){
    perror("failed open input file");
    return -1;
  }
  fseek(f, 0, SEEK_END);
  size_t len=ftell(f);
  fseek(f, 0, SEEK_SET);
  char* source=malloc(len+1);
  memset(source, 0, len+1);
  size_t read=fread(source, len, 1, f);
  if (read<1) {
    printf("error while reading file:%zu<%zu.\n",read,1l);
    free(source);
    return -2;
  }
  list_t tokens=do_lex(source);
  // free the source
  free(source);
#ifdef DEBUG
  for (size_t i=0; i < tokens.len; ++i) {
    token_t* tok=list_get(&tokens, i);
    printf("(%d, %s, line %zu, col %zu)\n",tok->token_type, tok->value,
	   tok->position.line, tok->position.column);
  }
#endif
  astnode_t *asttree=do_parse(&tokens);
  
  if(!asttree||!do_sematic(asttree)){
    return -1;
  }
  
#ifdef DEBUG
  print_node(asttree, 0);
#endif
  list_t intercodes=gen_intercode(asttree);
#ifdef DEBUG
  for (size_t i=0; i < intercodes.len; ++i) {
    intercode_t* code=list_get(&intercodes, i);
    printf("%s %s,%s,%s\n", codetype_tostr(code->type), code->varname, code->operand2str, code->store_var);
  }
#endif
  char* asmcode=asm_gen(&intercodes);
  
  
  if(!asmcode){
    return -1;
  }
  LOG(VERBOSE, "asm:\n%s",asmcode);
  
  FILE* fw=fopen(output, "w");
  if(!fw){
    perror("failed open output file");
    return -1;
  }
  fwrite(asmcode, strlen(asmcode), 1, fw);
  fclose(fw);
  
  //release the used mem
  free(asmcode);
  free_rest();
  return 0;
}
