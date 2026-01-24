#include "main.h"
#include "stdio.h"
#include "lexer.h"
#include "parser.h"
#include "sematic.h"
#include "intercode.h"
#include "intercode2.h"
#include "asmgen.h"
#include "utils.h"
#include <corecrt.h>
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
  char* input=NULL;
  char* output="a.asm";
  char* arch_str="amd64";
  char* abi_str="systemv";
  for (int i=1; i<argc; i++) {
    char* arg=argv[i];
    if(strcmp(arg, "-h")==0){
      // help
      printf("usage: GMS_COMPILER -i <input_file> [args..]\n");
      printf("available arguments:\n");
      printf("-o <output_file>\t set the output file name. Set to a.asm if not provided.\n"
	     "-m <arch>\t select the target architecture\n"
	     "--abi <abi>\t select the ABI to use\n");
    }else if (strcmp(arg, "-i")==0) {
      i++;
      if(i>=argc){
	printf("error: need input file after -i\n"); 
	return 0;
      }
      input=argv[i];
    }else if (strcmp(arg, "-o")==0) {
      i++;
      if(i>=argc){
	printf("error: need output file after -o\n");
	return 0;
      }
      input=argv[i];
    }else if (strcmp(arg, "--abi")==0) {
      i++;
      if(i>=argc){
	printf("error: need ABI after --abi\n");
	return 0;
      }
      abi_str=argv[i];
    }else if (strcmp(arg, "-m")==0) {
      i++;
      if(i>=argc){
	printf("error: need architecture after -m\n");
	return 0;
      }
      arch_str=argv[i];
    }
  }
  if(!input){
    printf("error: no input file\n");
    return 0;
  }
  abitype_t abi=ABI_SYSTEMV;
  if(strcmp(abi_str, "systemv")==0){
    abi=ABI_SYSTEMV;
  }else if(strcmp(abi_str, "microsoft")==0){
    abi=ABI_MICROSOFT;
  }else{
    printf("error: unknown abi format\n");
    return 0;
  }
  arch_t arch=ARCH_AMD64;
  if(strcmp(arch_str, "amd64")==0){
    arch=ARCH_AMD64;
  }else if(strcmp(arch_str, "aarch64")==0){
    arch=ARCH_AARCH64;
  }else{
    printf("error: unknown architecture format\n");
    return 0;
  }
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
    printf("(%s, %s, line %zu, col %zu)\n",tokentype_tostr(tok->token_type), tok->value,
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
  list_t ic1=gen_intercode(asttree);
  list_t intercodes=process_intercode(&ic1);
#ifdef DEBUG
  for (size_t i=0; i < intercodes.len; ++i) {
    intercode_t* code=list_get(&intercodes, i);
    printf("%s %s,%s,%s\n", codetype_tostr(code->type), code->op1.value, code->op2.value, code->op3.value);
  }
#endif
  platform_info_t platform={
    .abi=abi,
    .architecture=arch
  };
  char* asmcode=amd64_gen(&intercodes,platform);
  
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
