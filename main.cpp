#include "main.h"
#include "lexer.h"
#include "parser.h"
#include "sematic.h"
#include "intercode.h"
#include "intercode2.h"
#include "asmgen.h"
#include "status.h"
#include "utils.h"
#include <fstream>
#include <cstdio>
#include <ios>
#include <stdlib.h>
#include <string.h>
#include <string>

using std::string;
int main(int argc, char** argv){
  if(argc<2){
    perror("error: need input file\n");
    return -1;
  }
  string input;
  char* output="a.asm";
  char* arch_str="amd64";
  char* abi_str="default";
  for (int i=1; i<argc; i++) {
    char* arg=argv[i];
    if(strcmp(arg, "-h")==0){
      // help
      printf("usage: GMS_COMPILER -i <input_file> [args..]\n");
      printf("available arguments:\n");
      printf("-o <output_file>\t set the output file name. Set as a.asm if not "
             "provided.\n"
             "-m <arch>\t select the target architecture\n"
             "\tavailable architectures: amd64 aarch64\n"
             "--abi <abi>\t select the ABI to use\n"
	     "\tavailable ABIs: systemv microsoft aarch64");
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
      output=argv[i];
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
    }else{
      printf("error: unrecognized option \'%s\'\n", argv[i]);
      return 0;
    }
  }
  if(input.empty()){
    printf("error: no input file\n");
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
  abitype_t abi = (arch == ARCH_AARCH64 ? ABI_AARCH64 : ABI_SYSTEMV);
  switch (arch) {
  case ARCH_AMD64: {
    if(strcmp(abi_str, "systemv")==0||strcmp(abi_str, "default")==0){
      abi=ABI_SYSTEMV;
    }else if(strcmp(abi_str, "microsoft")==0){
      abi=ABI_MICROSOFT;
    }else{
      printf("error: unknown abi under this architecture\n");
      return 0;
    }
    break;
  }
  case ARCH_AARCH64: {
    if (strcmp(abi_str, "default")==0||strcmp(abi_str, "aarch64")==0) {
      abi=ABI_AARCH64;
    }else{
      printf("error: unknown abi under this architecture\n");
      return 0;
    }
    break;
  }
  default:
    printf("error: unknown abi format\n");
    return 0;
  }
  std::ifstream f(input,std::ios::in);
  if(f.fail()){
    printf("failed to open input file %s",input.c_str());
    perror(":");
    return -1;
  }
  f.seekg(std::ios::end);
  size_t len=f.tellg();
  f.seekg(std::ios::beg);
  string source;
  f>>source;
  f.close();
  // get the path of the directory of the source code.
  // we need to search for the included files based on this path. 
  std::string source_dir = input.substr(0,input.find_last_of('/'));
  /******************************************/
  /* if (read<1) {			    */
  /*   perror("error while reading file."); */
  /*   free(source);			    */
  /*   return -2;			    */
  /* }					    */
  /******************************************/
  compiler_global_data_t globals;
  init_compiler_global_data(&globals);

  int retv=0;
  lexer_result_t tokens = do_lex(source);
  // if (!preprocess_tokens(&tokens,source_dir)) {
  //   retv=-1;
  //   goto free_source;
  // }
  // free the source_dir variable
  parser_result_t asttree = do_parse(&tokens.tokenstream);
  if (!asttree.success) {
    return -1;
  }
  // if (is_log_part_enabled(PARSER_OUTPUTTREE)) {
  //   // print_node(asttree, 0);
  // }
  bool sematic_res=do_sematic(asttree.ast);
  if (!sematic_res) {
    // print_node(asttree, 0);
    printf("compilation failed\n");    
    return -1;
  }
  // TODO : gen qbe ir
  string ir_code;
  // do_log(VERBOSE, ASMGEN_RESULT, "asm:\n%s",asmcode);
  std::ofstream fw(output);
  if(fw.fail()){
    perror("failed open output file");
    goto free_asmcode;
  }
  fw<<ir_code;
  fw.close();

free_asmcode:
free_ir:
free_sematic:
free_ast:
free_tokens:
free_source:
  // free the source
  return 0;
}
